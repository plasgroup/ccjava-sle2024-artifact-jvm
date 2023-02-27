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
#include <map>
#include <utility>

class NVMCardTableWriteBarrierStub : public CodeStub {
  friend class NVMCardTableBarrierSetC1;

 private:
  LIR_Opr _obj;
  LIR_Opr _addr;
  LIR_Opr _value;
  address      _runtime_stub;
  BasicType _type;

 public:
  // addr (the address of the object head) and value must be registers.
  NVMCardTableWriteBarrierStub(LIR_Opr obj, LIR_Opr addr, LIR_Opr value,
                              address runtime_stub, BasicType type)
      : _obj(obj),
        _addr(addr),
        _value(value),
        _runtime_stub(runtime_stub),
        _type(type) {
          assert(value->is_register(), "value should be register");
          assert(addr->is_register(), "addr should be register");
          assert(obj->is_register(), "obj should be register");
        }

  LIR_Opr obj() const { return _obj; }
  LIR_Opr addr() const { return _addr; }
  LIR_Opr value() const { return _value; }
  address runtime_stub() const { return _runtime_stub; }
  // DecoratorSet decorators() const { return _decorators; }
  BasicType type() const { return _type; }

  virtual void emit_code(LIR_Assembler* ce);
  virtual void visit(LIR_OpVisitState* visitor) {
    // don't pass in the code emit info since it's processed in the fast path
    visitor->do_slow_case();
    visitor->do_input(_obj);
    visitor->do_input(_addr);
    visitor->do_input(_value);
  }
#ifndef PRODUCT
  virtual void print_name(outputStream* out) const { out->print("NVMPostBarrierStub"); }
#endif // PRODUCT
};

class CodeBlob;

class NVMCardTableBarrierSetC1 : public CardTableBarrierSetC1 {
  using parent = CardTableBarrierSetC1;
private:
  // a mark
  const int decorator_base_sz_ {2};
  const DecoratorSet decorators_[2] = {537141312ULL,538189888ULL};
  const int runtime_stubs_sz_ {256};
  address runtime_stubs[256];

  int get_runtime_stub_index(DecoratorSet decorators, BasicType type) {
    assert(((decorators & (decorators_[0])) == decorators_[0]) || 
           ((decorators & (decorators_[1])) == decorators_[1]), " unknown decorator base");
    //    base              |  Our Persist |  type
    //    log2(base_sz_)    |     3        |  4    
    //    8
    int idx = type - T_BOOLEAN;
    int i = 4;
    for (DecoratorSet bit: {OURPERSIST_DURABLE_ANNOTATION, OURPERSIST_IS_VOLATILE, OURPERSIST_IS_STATIC}) {
      if ((decorators & bit) != 0) {
        idx |= (1 << i);
      }
      i++;
    }

    if ((decorators & (decorators_[1])) == decorators_[1]) {
      idx |= (1 << i);
    }
    return idx;
  }

protected:

  virtual void store_at_resolved(LIRAccess& access, LIR_Opr value);
  // virtual LIR_Opr atomic_cmpxchg_at_resolved(LIRAccess& access, LIRItem& cmp_value, LIRItem& value) {
  //   access.gen()->bailout("not now");return nullptr;
  //   return parent::atomic_cmpxchg_at_resolved(access, cmp_value, value);
  // }
  virtual void nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr value);

public:

  NVMCardTableBarrierSetC1() {
    for (int i = 0; i < runtime_stubs_sz_; i++) {
      runtime_stubs[i] = nullptr; 
    }
  }

  void generate_c1_runtime_stubs(BufferBlob* buffer_blob);

  void insert_runtime_stub(DecoratorSet decorators, BasicType type, address stub) {
    int idx = get_runtime_stub_index(decorators, type);
    runtime_stubs[idx] = stub;

  }
  address get_runtime_stub(DecoratorSet decorators, BasicType type) {
    int idx = get_runtime_stub_index(decorators, type);
    assert(runtime_stubs[idx] != nullptr, "should not be nullptr");
    return runtime_stubs[idx];
  }
  // CodeBlob* post_barrier_c1_runtime_code_blob() { return _post_barrier_c1_runtime_code_blob; }
  void print_info(LIRAccess& access, LIR_Opr value) {
    static int cnt = 0;
    bool is_array = (access.decorators() & IS_ARRAY) != 0;
    tty->print("= = = = Store at Resolved  %d = = = =\n", cnt);
    cnt++;
    tty->print("decorator = %ld\n", access.decorators());
    tty->print("type = %s, %s\n", type2name(access.type()), is_array ? "array" : "");
    
    access.base().item().result()->print();         tty->print("\n");
    access.offset().opr()->print();         tty->print("\n");
    access.resolved_addr()->print();          tty->print("\n");
    tty->print("\n\n");
  }
    


  LIR_Opr ensure_in_register(LIRGenerator* gen, LIR_Opr opr, BasicType type) {
  if (!opr->is_register()) {
    LIR_Opr opr_reg;
    if (opr->is_constant()) {
      opr_reg = gen->new_register(type);
      gen->lir()->move(opr, opr_reg);
    } else {
      opr_reg = gen->new_pointer_register();
      gen->lir()->leal(opr, opr_reg);
    }
    opr = opr_reg;
  }
  return opr;
}
};
#endif // SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP
