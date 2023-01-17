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
  LIR_Opr _offset;
  LIR_Opr _new_val;
  address      _runtime_stub;

 public:
  // addr (the address of the object head) and new_val must be registers.
  NVMCardTableWriteBarrierStub(LIR_Opr obj, LIR_Opr offset, LIR_Opr new_val,
                              address runtime_stub)
      : _obj(obj),
        _offset(offset),
        _new_val(new_val),
        _runtime_stub(runtime_stub) {}

  LIR_Opr obj() const { return _obj; }
  LIR_Opr offset() const { return _offset; }
  LIR_Opr new_val() const { return _new_val; }
  address runtime_stub() const { return _runtime_stub; }
  // DecoratorSet decorators() const { return _decorators; }
  // BasicType type() const { return _basic_type; }

  virtual void emit_code(LIR_Assembler* ce);
  virtual void visit(LIR_OpVisitState* visitor) {
    // don't pass in the code emit info since it's processed in the fast path
    visitor->do_slow_case();
    visitor->do_input(_obj);
    visitor->do_input(_new_val);
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
  address runtime_stubs[2 << 10];

protected:

  virtual void store_at_resolved(LIRAccess& access, LIR_Opr value);

  // virtual LIR_Opr atomic_cmpxchg_at_resolved(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value) {
  //   access.gen()->bailout("not now");return nullptr;
  //   return parent::atomic_cmpxchg_at_resolved(access, cmp_value, new_value);
  // }

  virtual void nvm_write_barrier(LIRAccess& access, LIR_Opr addr, LIR_Opr new_val);

public:

  NVMCardTableBarrierSetC1() {
    }

  void generate_c1_runtime_stubs(BufferBlob* buffer_blob);

  void insert_runtime_stub(DecoratorSet decorators, BasicType type, address stub) {
    // auto key = std::make_pair(decorators, type);
    // runtime_stubs.insert({key, stub});

    // std::map<std::pair<DecoratorSet, BasicType>, address>::value_type;
    // runtime_stubs.insert(std::map<std::pair<DecoratorSet, BasicType>, address>::value_type{{std::make_pair(decorators, type)}, stub});
    // const std::pair<DecoratorSet, BasicType> key {decorators,  type};
    // const std::map<std::pair<DecoratorSet, BasicType>, address>::value_type & kv {key, stub};
    // runtime_stubs.insert(kv);
    // runtime_stubs.insert(std::pair{std::pair{decorators, type}, stub});
    // const std::pair<DecoratorSet, BasicType> key {std::pair<DecoratorSet, BasicType>(decorators, type)};
    // std::map<std::pair<DecoratorSet, BasicType>, address>::value_type kv { std::pair<const std::pair<DecoratorSet, BasicType>, address>(key, stub)};
    // // runtime_stubs.insert(kv);
    // runtime_stubs.insert((const std::map<std::pair<DecoratorSet, BasicType>, address>::value_type &)kv);
    // runtime_stubs.insert(kv);
    int idx = type - T_BOOLEAN;
    int i = 4;
    if (decorators & OURPERSIST_DURABLE_ANNOTATION != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & OURPERSIST_IS_VOLATILE != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & OURPERSIST_IS_STATIC != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & 270400ULL == 270400ULL) {
      idx &= (1 << i); i++;
    }
    if (decorators & 270464ULL == 270464ULL) {
      idx &= (1 << i); i++;
    }
    runtime_stubs[idx] = stub;

  }
  address get_runtime_stub(DecoratorSet decorators, BasicType type) {
    //    base |  Our Persist |  type
    //    3    |     3        |  4    
    //
    int idx = type - T_BOOLEAN;
    int i = 4;
    if (decorators & OURPERSIST_DURABLE_ANNOTATION != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & OURPERSIST_IS_VOLATILE != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & OURPERSIST_IS_STATIC != 0) {
      idx &= (1 << i); i++;
    }
    if (decorators & 270400ULL == 270400ULL) {
      idx &= (1 << i); i++;
    }
    if (decorators & 270464ULL == 270464ULL) {
      idx &= (1 << i); i++;
    }
    return runtime_stubs[idx];
    // auto key = std::make_pair(decorators, type);
    // auto key = decorators;
    // auto it = runtime_stubs.find(key);
    // if (it == runtime_stubs.end()) {
    //   return 0;
    // }
    // return it->second;
    // return runtime_stubs[{decorators, type}];
    // return runtime_stubs[std::make_pair(decorators, type)];
  };
  // CodeBlob* post_barrier_c1_runtime_code_blob() { return _post_barrier_c1_runtime_code_blob; }

};
#endif // SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP
