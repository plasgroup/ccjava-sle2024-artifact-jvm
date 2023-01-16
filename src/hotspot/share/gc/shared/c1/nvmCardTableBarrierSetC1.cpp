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

void NVMCardTableBarrierSetC1::store_at_resolved(LIRAccess& access, LIR_Opr value) {
  DecoratorSet decorators = access.decorators();
  bool is_array = (decorators & IS_ARRAY) != 0;
  bool on_anonymous = (decorators & ON_UNKNOWN_OOP_REF) != 0;
  bool needs_wupd = (decorators & OURPERSIST_NEEDS_WUPD) != 0;
  // log
  printf("#Access Information:\n\
type = %s\n\
needs_wupd = %s\n\n\n", 
  type2name(access.type()), needs_wupd ? "true" : "false");
  // bailout
  bool C1_nvm_have_implemented = access.is_oop();
  // bool C1_nvm_have_implemented = false;
  if (!C1_nvm_have_implemented) {
    printf("bail out = true\n");
    access.gen()->bailout("not now");
    return;
  }
  printf("bail out = false\n");
  
  parent::store_at_resolved(access, value);

  if (needs_wupd) {
    // printf("nvm writer: type = %s\n",  
    //   type2name(access.type())
    // );
    nvm_write_barrier(access, access.resolved_addr(), value);
  }
}

void NVMCardTableBarrierSetC1::nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val) {
  LIRGenerator* gen = access.gen();

  BasicType flag_type = T_INT;
  LIR_Address* mark_durable_flag_addr =
    new LIR_Address(access.base().opr(),
                    oopDesc::nvm_header_offset_in_bytes(),
                    flag_type);
  LIR_Opr flag_val = gen->new_register(T_INT);
  __ load(mark_durable_flag_addr, flag_val);
  __ cmp(lir_cond_notEqual, flag_val, LIR_OprFact::intConst(0));

  const address runtime_stub =  runtime_stubs[{access.decorators(), access.type()}];
  CodeStub* const slow = new NVMCardTableWriteBarrierStub(access.base().opr(), access.offset().opr(), new_val, runtime_stub);

  __ branch(lir_cond_notEqual, slow);
  __ branch_destination(slow->continuation());

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
    nvm_bsa->generate_c1_write_barrier_runtime_stub(sasm, _decorators, _type);
    return NULL;
  }
};

static address generate_c1_runtime_stub(BufferBlob* blob, DecoratorSet decorators, BasicType type, const char* name) {
  NVMWriteBarrierRuntimeStubCodeGenClosure cl(decorators, type);
  CodeBlob* const code_blob = Runtime1::generate_blob(blob, -1 /* stub_id */, name, false /* expect_oop_map*/, &cl);
  return code_blob->code_begin();
}

void NVMCardTableBarrierSetC1::generate_c1_runtime_stubs(BufferBlob* buffer_blob) {
  // printf("enter NVMCardTableBarrierSetC1::generate_c1_runtime_stubs\n");
  // _write_barrier_on_oop_field_1_runtime_stub =
  //   generate_c1_runtime_stub(blob, ON_STRONG_OOP_REF, "_write_barrier_on_oop_field_1_runtime_stub");
  for (DecoratorSet base : {1318976ULL, 270400ULL, 270464ULL}) {
    for (DecoratorSet if_static: {OURPERSIST_IS_STATIC, OURPERSIST_IS_NOT_STATIC}) {
      for (DecoratorSet if_volatile: {OURPERSIST_IS_VOLATILE, OURPERSIST_IS_NOT_VOLATILE}) {
        for (DecoratorSet if_durable: {OURPERSIST_DURABLE_ANNOTATION, OURPERSIST_NOT_DURABLE_ANNOTATION}) {
          DecoratorSet ds = OURPERSIST_BS_ASM | base | if_static | if_volatile | if_durable;
          // address adr;
          // switch (type) {
          // case T_BOOLEAN: adr = generate_c1_runtime_stub(blob, ds, jboolean, ""); break;
          // case T_CHAR:    adr = generate_c1_runtime_stub(blob, ds, jchar   , ""); break;
          // case T_FLOAT:   adr = generate_c1_runtime_stub(blob, ds, jfloat  , ""); break;
          // case T_DOUBLE:  adr = generate_c1_runtime_stub(blob, ds, jdouble , ""); break;
          // case T_BYTE:    adr = generate_c1_runtime_stub(blob, ds, jbyte   , ""); break;
          // case T_SHORT:   adr = generate_c1_runtime_stub(blob, ds, jshort  , ""); break;
          // case T_INT:     adr = generate_c1_runtime_stub(blob, ds, jint    , ""); break;
          // case T_LONG:    adr = generate_c1_runtime_stub(blob, ds, jlong   , ""); break;
          // case T_ARRAY:
          // case T_OBJECT:  adr = generate_c1_runtime_stub(blob, ds, jobject , ""); break;
          // default: ShouldNotReachHere();
          // }
          for (BasicType type: {T_BOOLEAN, T_CHAR, T_FLOAT,T_DOUBLE, T_BYTE, T_SHORT, T_INT, T_LONG, T_ARRAY, T_OBJECT}) {
            runtime_stubs[std::make_pair(ds, type)] = generate_c1_runtime_stub(buffer_blob, ds, type, "");
          }
        }
      }
    }
  }
  // _post_barrier_c1_runtime_code_blob->print();
  // puts("exit NVMCardTableBarrierSetC1::generate_c1_runtime_stubs");
}