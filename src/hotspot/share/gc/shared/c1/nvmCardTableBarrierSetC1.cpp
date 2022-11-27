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

#ifdef COMPILER1
#include "c1/c1_LIRAssembler.hpp"
#include "c1/c1_MacroAssembler.hpp"
#endif

#ifdef ASSERT
#define __ gen->lir(__FILE__, __LINE__)->
#else
#define __ gen->lir()->
#endif
void NVMCardTablePostBarrierStub::emit_code(LIR_Assembler* ce) {
  NVMCardTableBarrierSetAssembler* bs = (NVMCardTableBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
  bs->gen_post_barrier_stub(ce, this);
}

void NVMCardTableBarrierSetC1::store_at_resolved(LIRAccess& access, LIR_Opr value) {
  // bailout
  access.gen()->bailout("not now");return;
  // decorators

  // puts("C1: store at resolved");

  DecoratorSet decorators = access.decorators();
  bool is_array = (decorators & IS_ARRAY) != 0;
  bool on_anonymous = (decorators & ON_UNKNOWN_OOP_REF) != 0;

  // call parent's method
  // pre_barrier
  parent::store_at_resolved(access, value);

  // post barrier
  bool precise = is_array || on_anonymous;
  LIR_Opr post_addr = precise ? access.resolved_addr() : access.base().opr();
  nvm_write_barrier(access, post_addr, value);
}

void NVMCardTableBarrierSetC1::nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val) {
  LIRGenerator* gen = access.gen();
  auto slow = new NVMCardTablePostBarrierStub(addr, new_val);


  __ branch(lir_cond_always, slow);
  __ branch_destination(slow->continuation());
}

class C1NVMPostBarrierCodeGenClosure : public StubAssemblerCodeGenClosure {
  virtual OopMapSet* generate_code(StubAssembler* sasm) {
    auto bs = (NVMCardTableBarrierSetAssembler*)BarrierSet::barrier_set()->barrier_set_assembler();
    bs->generate_c1_post_barrier_runtime_stub(sasm);
    return NULL;
  }
};

void NVMCardTableBarrierSetC1::generate_c1_runtime_stubs(BufferBlob* buffer_blob) {
  // printf("enter NVMCardTableBarrierSetC1::generate_c1_runtime_stubs\n");

  C1NVMPostBarrierCodeGenClosure post_code_gen_cl;
  _post_barrier_c1_runtime_code_blob = Runtime1::generate_blob(buffer_blob, -1, "nvm_post_barrier", false, &post_code_gen_cl);

  // _post_barrier_c1_runtime_code_blob->print();
  // puts("exit NVMCardTableBarrierSetC1::generate_c1_runtime_stubs");
}


// void NVMCardTableBarrierSetC1::store_at(LIRAccess& access, LIR_Opr value) {
//   access.gen()->bailout("not now");
// }

// void NVMCardTableBarrierSetC1::load_at(LIRAccess& access, LIR_Opr result) {
//   access.gen()->bailout("not now");
// }

// void NVMCardTableBarrierSetC1::load(LIRAccess& access, LIR_Opr result) {
//   access.gen()->bailout("not now");
// }


// LIR_Opr NVMCardTableBarrierSetC1::atomic_cmpxchg_at(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value) {
//   access.gen()->bailout("not now"); return nullptr;
//   return parent::atomic_cmpxchg_at(access, cmp_value, new_value);
// }


// LIR_Opr NVMCardTableBarrierSetC1::atomic_xchg_at(LIRAccess& access, LIRItem& value) {
//   access.gen()->bailout("not now"); return nullptr;
//   return parent::atomic_xchg_at(access, value);
// }

// LIR_Opr NVMCardTableBarrierSetC1::atomic_add_at(LIRAccess& access, LIRItem& value) {
//   access.gen()->bailout("not now");return nullptr;
//   return parent::atomic_add_at(access, value);
// }

// void NVMCardTableBarrierSetC1::nvm_write_barrier_lir(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val) {
//   // access.gen()->bailout("not now"); return;
//   static int cnt = 0;
//   LIRGenerator* gen = access.gen();
  
//   address func = CAST_FROM_FN_PTR(address, Runtime1::nvm_print);

//   BasicTypeList signature(1);
//   signature.append(T_INT);
//   CallingConvention* cc = gen->frame_map()->c_calling_convention(&signature);

//   __ move(LIR_OprFact::intConst(cnt++), cc->at(0));
//   __ call_runtime_leaf(func, LIR_OprFact::illegalOpr, LIR_OprFact::illegalOpr, cc->args());
// }