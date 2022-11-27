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

#ifndef SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP
#define SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"

class NVMCardTablePostBarrierStub: public CodeStub {
  friend class NVMCardTableBarrierSetC1;
 private:
  LIR_Opr _addr;
  LIR_Opr _new_val;

 public:
  // addr (the address of the object head) and new_val must be registers.
  NVMCardTablePostBarrierStub(LIR_Opr addr, LIR_Opr new_val): _addr(addr), _new_val(new_val) { }

  LIR_Opr addr() const { return _addr; }
  LIR_Opr new_val() const { return _new_val; }

  virtual void emit_code(LIR_Assembler* ce);
  virtual void visit(LIR_OpVisitState* visitor) {
    // don't pass in the code emit info since it's processed in the fast path
    visitor->do_slow_case();
    visitor->do_input(_addr);
    visitor->do_input(_new_val);
  }
#ifndef PRODUCT
  virtual void print_name(outputStream* out) const { out->print("NVMPostBarrierStub"); }
#endif // PRODUCT
};

class CodeBlob;

class NVMCardTableBarrierSetC1 : public CardTableBarrierSetC1 {
  using parent = CardTableBarrierSetC1;
protected:
  CodeBlob* _post_barrier_c1_runtime_code_blob;

  virtual LIR_Opr resolve_address(LIRAccess& access, bool resolve_in_register) {
    access.gen()->bailout("not now");return nullptr;
    return parent::resolve_address(access, resolve_in_register);
  }

  virtual void generate_referent_check(LIRAccess& access, LabelObj* cont) {
    access.gen()->bailout("not now");
  }

  virtual void store_at_resolved(LIRAccess& access, LIR_Opr value);

  virtual void load_at_resolved(LIRAccess& access, LIR_Opr result) {
    access.gen()->bailout("not now");
  }

  virtual LIR_Opr atomic_cmpxchg_at_resolved(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value) {
    access.gen()->bailout("not now");return nullptr;
    return parent::atomic_cmpxchg_at_resolved(access, cmp_value, new_value);
  }

  virtual LIR_Opr atomic_xchg_at_resolved(LIRAccess& access, LIRItem& value) {
    access.gen()->bailout("not now");return nullptr;
    return parent::atomic_xchg_at_resolved(access, value);
  }

  virtual LIR_Opr atomic_add_at_resolved(LIRAccess& access, LIRItem& value) {
    access.gen()->bailout("not now");return nullptr;
    return parent::atomic_add_at_resolved(access, value);
  }

  // virtual void nvm_write_barrier_lir(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val);

  virtual void nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val);

  virtual void post_barrier(LIRAccess& access, LIR_OprDesc* addr, LIR_OprDesc* new_val) {
    access.gen()->bailout("not now");
  }

public:
  NVMCardTableBarrierSetC1()
    : _post_barrier_c1_runtime_code_blob(NULL) {
      puts("NVMCardTableBarrierSetC1");
    }

  void generate_c1_runtime_stubs(BufferBlob* buffer_blob);
  CodeBlob* post_barrier_c1_runtime_code_blob() { return _post_barrier_c1_runtime_code_blob; }

  // virtual void store_at(LIRAccess& access, LIR_Opr result) {
  //   puts("C1 store_at");
  // }
  // virtual void load_at(LIRAccess& access, LIR_Opr result) {
  //   puts("C load_at");
  // }
  // virtual void load(LIRAccess& access, LIR_Opr result);

  // virtual LIR_Opr atomic_cmpxchg_at(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value);

  // virtual LIR_Opr atomic_xchg_at(LIRAccess& access, LIRItem& value);

  // virtual LIR_Opr atomic_add_at(LIRAccess& access, LIRItem& value);

  // virtual LIR_Opr resolve(LIRGenerator* gen, DecoratorSet decorators, LIR_Opr obj) {
  //   gen->bailout("not now");return nullptr;
  //   return parent::resolve(gen, decorators, obj);
  // }


};
#endif // SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP
