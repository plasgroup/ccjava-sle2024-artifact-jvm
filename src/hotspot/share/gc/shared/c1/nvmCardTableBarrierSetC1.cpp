/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */
#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "gc/shared/c1/nvmCardTableBarrierSetC1.hpp"
#include "gc/shared/cardTable.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSetAssembler.hpp"
#include "utilities/macros.hpp"
#include "runtime/arguments.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/stubRoutines.hpp"
#include "runtime/vm_version.hpp"
#include "c1/c1_Runtime1.hpp"
#include "c1/c1_CodeStubs.hpp"
#include "nvm/ourPersist.hpp"
#ifdef COMPILER1
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#include <unordered_map>
#include <utility>
#endif

#ifdef ASSERT
#define __ gen->lir(__FILE__, __LINE__)->
#else
#define __ gen->lir()->
#endif
void NVMCardTableWriteBarrierStub::emit_code(LIR_Assembler* ce) {
  NVMCardTableBarrierSetAssembler* bs = (NVMCardTableBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
  bs->gen_write_barrier_stub(ce, this);
}

void NVMCardTableWriteBarrierAtomicStub::emit_code(LIR_Assembler* ce) {
  NVMCardTableBarrierSetAssembler* bs = (NVMCardTableBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
  bs->gen_write_barrier_atomic_stub(ce, this);
}

void NVMCardTableBarrierSetC1::store_at_resolved(LIRAccess& access, LIR_Opr value) {
  // note that 86 64 defined

  const DecoratorSet decorators = access.decorators();
  bool needs_patching {(decorators & C1_NEEDS_PATCHING) != 0};
  bool needs_wupd {(decorators & OURPERSIST_NEEDS_WUPD) != 0};

  if (needs_patching) {
    access.gen()->bailout("unsupported");

    #ifdef ASSERT
    static int count{0};
    printf("%d th needs_patching\n", ++count);
    #endif
    
    return;
  }
  
  if (!needs_wupd) {
    parent::store_at_resolved(access, value);
    return;
  }

  nvm_write_barrier(access, access.resolved_addr(), value);
 
}

void NVMCardTableBarrierSetC1::nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr value) {

  LIRGenerator* gen = access.gen();
  bool is_array = (access.decorators() & IS_ARRAY) != 0;
  bool is_volatile = (access.decorators() & MO_SEQ_CST) != 0;
  bool needs_sync {access.is_oop() && ((access.decorators() & OURPERSIST_NEEDS_SYNC) != 0)};

  access.clear_decorators(OURPERSIST_NEEDS_SYNC);

  assert(!access.is_oop() || (access.is_oop() && needs_sync), "for now");


  LabelObj* done = nullptr;
  // object
  LIRItem& base = access.base().item();

  if (!is_volatile) {
    parent::store_at_resolved(access, value);

    // replica
    done =  new LabelObj();
    LIR_Opr replica = gen->new_register(T_ADDRESS);
    LIR_Address* replica_addr = new LIR_Address(base.result(), oopDesc::nvm_header_offset_in_bytes(), T_ADDRESS);
    __ move(replica_addr, replica);
    // clear last three bits
    __ logical_and(replica, LIR_OprFact::intConst(~0b111), replica);
    __ cmp(lir_cond_equal, replica, LIR_OprFact::intConst(0));
    __ branch(lir_cond_equal, done->label());

  }
  // field address
  if (addr->is_address()) {
    LIR_Address* address = addr->as_address_ptr();

    LIR_Opr ptr = gen->new_pointer_register();
    if (!address->index()->is_valid() && address->disp() == 0) {
      __ move(address->base(), ptr);
    } else {
      assert(address->disp() != max_jint, "lea doesn't support patched addresses!");
      __ leal(addr, ptr);
    }
    addr = ptr;
  }
  assert(addr->is_register(), "must be a register at this point");

  // value
  value = ensure_in_register(gen, value, is_subword_type(access.type()) ? T_INT : access.type());
  // runtime stub
  const address runtime_stub = get_runtime_stub(access.decorators(), access.type(), needs_sync, is_volatile, false);
  
  CodeStub* const slow = new NVMCardTableWriteBarrierStub(base.result(), addr, value, runtime_stub, access.type(), access.access_emit_info(), needs_sync);

  __ branch(lir_cond_always, slow);
  
  if (!is_volatile) {
    __ branch_destination(done->label());
  }
  
  __ branch_destination(slow->continuation());
}

LIR_Opr NVMCardTableBarrierSetC1::atomic_cmpxchg_at_resolved(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value) {
  LIRGenerator* gen = access.gen();
  bool needs_sync {access.is_oop()};

  printf("generate stub for atomic: decorator=%ld, type = %s\n", access.decorators(), type2name(access.type()));
  // object
  LIRItem& base = access.base().item();
  const address runtime_stub = get_runtime_stub(access.decorators(), access.type(), needs_sync, false, true);
  
  // field address
  LIR_Opr addr = access.resolved_addr();
  if (addr->is_address()) {
    LIR_Address* address = addr->as_address_ptr();

    LIR_Opr ptr = gen->new_pointer_register();
    if (!address->index()->is_valid() && address->disp() == 0) {
      __ move(address->base(), ptr);
    } else {
      assert(address->disp() != max_jint, "lea doesn't support patched addresses!");
      __ leal(addr, ptr);
    }
    addr = ptr;
  }
  assert(addr->is_register(), "must be a register at this point");
  // value
  if (access.type() == T_LONG) {
    cmp_value.load_item_force(FrameMap::long0_opr);
    new_value.load_item_force(FrameMap::long1_opr);
  } else {
    cmp_value.load_item_force(FrameMap::rax_oop_opr);
    new_value.load_item();
  }


  LIR_Opr result = gen->new_register(T_INT);

  CodeStub* const slow = new NVMCardTableWriteBarrierAtomicStub(lir_cas_int /* for cas, we don't need fine-grained code. lir_cas_int is used for object, long too*/, base.result(), addr, new_value.result(), runtime_stub, access.type(), cmp_value.result(), result);

  __ branch(lir_cond_always, slow);
    
  __ branch_destination(slow->continuation());

  return result;
  // replica
  // __ cmove(lir_cond_equal, LIR_OprFact::intConst(1), LIR_OprFact::intConst(0),
          //  result, T_INT);

  // printf("%ld\n", access.decorators());
  // puts("executed");
  // // LIRItem& base = access.base().item();
  // LIRGenerator* gen = access.gen();
  // LIRItem& base = access.base().item();
  // LabelObj* retry = new LabelObj();

  // // replica
  // LIR_Opr replica = gen->new_register(T_INT);
  // LIR_Address* replica_addr = new LIR_Address(base.result(), oopDesc::nvm_header_offset_in_bytes(), T_INT);
  
  // __ branch_destination(retry->label());
  // __ move(replica_addr, replica);
  // LIR_Opr cmp = gen->new_register(T_INT);
  // LIR_Opr val = gen->new_register(T_INT);
  // __ move(replica, cmp);
  // __ move(replica, val);
  
  // __ logical_and(cmp, LIR_OprFact::intConst(~0b1), cmp);
  // __ logical_or(val, LIR_OprFact::intConst(0b1), val);

  
  // __ cas_int(LIR_OprFact::address(replica_addr), cmp, val, LIR_OprFact::illegalOpr, LIR_OprFact::illegalOpr, LIR_OprFact::illegalOpr);

  // // clear last three bits
  // // __ logical_and(replica, LIR_OprFact::intConst(~0b111), replica);
  // // // LIR_OprFact::intptrConst(NULL_WORD) does not work in this branch
  // // __ cmp(lir_cond_equal, replica, LIR_OprFact::intConst(0));
  // __ branch(lir_cond_notEqual, retry->label());
  
  // return BarrierSetC1::atomic_cmpxchg_at_resolved(access, cmp_value, new_value);
}

LIR_Opr NVMCardTableBarrierSetC1::atomic_xchg_at_resolved(LIRAccess& access, LIRItem& value) {
  assert(false, "check");
  // LIRItem& base = access.base().item();
  return BarrierSetC1::atomic_xchg_at_resolved(access, value);
}

LIR_Opr NVMCardTableBarrierSetC1::atomic_add_at_resolved(LIRAccess& access, LIRItem& value) {
  LIRGenerator* gen = access.gen();

  printf("generate stub for atomic add at: decorator=%ld, type = %s\n", access.decorators(), type2name(access.type()));
  // object
  LIRItem& base = access.base().item();
  const address runtime_stub = get_runtime_stub(access.decorators(), access.type(), false, false, false);
  
  // field address
  LIR_Opr addr = access.resolved_addr();
  if (addr->is_address()) {
    LIR_Address* address = addr->as_address_ptr();

    LIR_Opr ptr = gen->new_pointer_register();
    if (!address->index()->is_valid() && address->disp() == 0) {
      __ move(address->base(), ptr);
    } else {
      assert(address->disp() != max_jint, "lea doesn't support patched addresses!");
      __ leal(addr, ptr);
    }
    addr = ptr;
  }
  assert(addr->is_register(), "must be a register at this point");
  // value
  value.load_item();
  LIR_Opr result = gen->new_register(access.type());

  __ move(value.result(), result);
  
  CodeStub* const slow = new NVMCardTableWriteBarrierAtomicStub(lir_xadd, base.result(), addr, result, runtime_stub, access.type(), LIR_OprFact::illegalOpr, result);

  __ branch(lir_cond_always, slow);
    
  __ branch_destination(slow->continuation());
  
  return result;
}

class NVMWriteBarrierRuntimeStubCodeGenClosure : public StubAssemblerCodeGenClosure {
private:
  const DecoratorSet _decorators;
  const BasicType _type;

public:
  NVMWriteBarrierRuntimeStubCodeGenClosure(DecoratorSet decorators, BasicType type) :
  _decorators(decorators), _type(type) {}

  virtual OopMapSet* generate_code(StubAssembler* sasm) {
    BarrierSetAssembler* const bsa = BarrierSet::barrier_set()->barrier_set_assembler();
    auto nvm_bsa = reinterpret_cast<NVMCardTableBarrierSetAssembler*>(bsa);
    if (_decorators == 805577728ULL || _decorators == 1100317205504ULL) {
      // specially for atomic
      puts("generate runtime stub for atomic");
      nvm_bsa->generate_c1_write_barrier_atomic_runtime_stub(sasm, _decorators, _type);
    } else {
      nvm_bsa->generate_c1_write_barrier_runtime_stub(sasm, _decorators, _type);
    }
    return NULL;
  }
};

static address generate_c1_runtime_stub(BufferBlob* blob, DecoratorSet decorators, BasicType type, const char* name) {
  NVMWriteBarrierRuntimeStubCodeGenClosure cl(decorators, type);
  CodeBlob* const code_blob = Runtime1::generate_blob(blob, -1 /* stub_id */, name, false /* expect_oop_map*/, &cl);
  return code_blob->code_begin();
}

void NVMCardTableBarrierSetC1::generate_c1_runtime_stubs(BufferBlob* buffer_blob) {
  for (int i = 0; i < 4; i++) {
    DecoratorSet base = decorators_[i];
    for (BasicType type: {T_BOOLEAN, T_CHAR, T_FLOAT,T_DOUBLE, T_BYTE, T_SHORT, T_INT, T_LONG, T_ARRAY, T_OBJECT}) {
      insert_runtime_stub(base, type, generate_c1_runtime_stub(buffer_blob, base, type, ""));
    }
  }

  // atomic compare and swap
  DecoratorSet base = decorators_[4];
  for (BasicType type: {T_INT, T_LONG}) {
    insert_runtime_stub(base, type, generate_c1_runtime_stub(buffer_blob, base, type, ""));
  }
  // atomic add at
  base = decorators_[5];
  for (BasicType type: {T_INT, T_LONG}) {
    insert_runtime_stub(base, type, generate_c1_runtime_stub(buffer_blob, base, type, ""));
  }


}
#endif
