#ifdef OUR_PERSIST

#ifndef SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP
#define SHARE_GC_SHARED_C1_NVMCARDTABLEBARRIERSETC1_HPP

#include "c1/c1_CodeStubs.hpp"
#include "c1/c1_Runtime1.hpp"
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
  CodeEmitInfo* _info;
  bool _needs_sync;

 public:
  // addr (the address of the object head) and value must be registers.
  NVMCardTableWriteBarrierStub(LIR_Opr obj, LIR_Opr addr, LIR_Opr value, address runtime_stub, BasicType type, CodeEmitInfo* info, bool needs_sync)
      : _obj(obj),
        _addr(addr),
        _value(value),
        _runtime_stub(runtime_stub),
        _type(type),
        _info(nullptr),
        _needs_sync(needs_sync) {
          assert(value->is_register(), "value should be register");
          assert(addr->is_register(), "addr should be register");
          assert(obj->is_register(), "obj should be register");
          // clone
          if (info != nullptr) {
            _info = new CodeEmitInfo(info);
          }
        }

  LIR_Opr obj() const { return _obj; }
  LIR_Opr addr() const { return _addr; }
  LIR_Opr value() const { return _value; }
  address runtime_stub() const { return _runtime_stub; }
  // DecoratorSet decorators() const { return _decorators; }
  BasicType type() const { return _type; }
  CodeEmitInfo* info() const { return _info; }
  bool needs_sync() const { return _needs_sync; }
  virtual void emit_code(LIR_Assembler* ce);
  virtual void visit(LIR_OpVisitState* visitor) {
    if (_info != nullptr) {
      visitor->do_slow_case(_info);
    } else {
      visitor->do_slow_case();
    }
    visitor->do_input(_obj);
    visitor->do_input(_addr);
    visitor->do_input(_value);
  }
#ifndef PRODUCT
  virtual void print_name(outputStream* out) const { out->print("NVMPostBarrierStub"); }
#endif // PRODUCT
};


class NVMCardTableWriteBarrierAtomicStub : public CodeStub {
  friend class NVMCardTableBarrierSetC1;

 private:
  LIR_Code _code;
  LIR_Opr _obj;
  LIR_Opr _addr;
  LIR_Opr _value;
  address      _runtime_stub;
  BasicType _type;
  LIR_Opr _cmp;
  LIR_Opr _res;
  CodeEmitInfo* _info;
  bool _needs_sync;
 public:
  // addr (the address of the object head) and value must be registers.
  NVMCardTableWriteBarrierAtomicStub(LIR_Code code, LIR_Opr obj, LIR_Opr addr, LIR_Opr value,
                              address runtime_stub, BasicType type, LIR_Opr cmp, LIR_Opr res, CodeEmitInfo* info, bool needs_sync)
      : _code(code),
        _obj(obj),
        _addr(addr),
        _value(value),
        _runtime_stub(runtime_stub),
        _type(type),
        _cmp(cmp),
        _res(res),
        _info(nullptr),
        _needs_sync(needs_sync) {
          assert(value->is_register(), "value should be register");
          assert(addr->is_register(), "addr should be register");
          assert(obj->is_register(), "obj should be register");
          assert((cmp->is_valid() && cmp->is_register()) || (!cmp->is_valid()), "cmp should be register or illegal");
          assert(res->is_register(), "res should be register");
          // clone
          assert(!is_reference_type(type) || (is_reference_type(type) && info != nullptr), "sanity check");
          
          if (info != nullptr) {
            _info = new CodeEmitInfo(info);
          }

        }

  CodeEmitInfo* info() const { return _info; }
  LIR_Code code() const { return _code; }
  LIR_Opr obj() const { return _obj; }
  LIR_Opr addr() const { return _addr; }
  LIR_Opr value() const { return _value; }
  address runtime_stub() const { return _runtime_stub; }
  // DecoratorSet decorators() const { return _decorators; }
  BasicType type() const { return _type; }
  LIR_Opr cmp() const { return _cmp; }
  LIR_Opr res() const { return _res; }
  bool needs_sync() const { return _needs_sync; }

  virtual void emit_code(LIR_Assembler* ce);
  virtual void visit(LIR_OpVisitState* visitor) {
    if (_info != nullptr) {
      assert(_code != lir_xadd, "sanity check");
      assert(_type != T_LONG && _type != T_INT, "sanity check");
      visitor->do_info(_info);
    }
    visitor->do_input(_obj);
    visitor->do_input(_addr);
    visitor->do_input(_value);
    if (_cmp != LIR_OprFact::illegalOpr) {
      assert(_code != lir_xadd, "sanity check");
      visitor->do_input(_cmp);
    }
    visitor->do_output(_res);
  }
#ifndef PRODUCT
  virtual void print_name(outputStream* out) const { out->print("NVMPostBarrierAtomicStub"); }
#endif // PRODUCT
};

class CodeBlob;

class NVMCardTableBarrierSetC1 : public CardTableBarrierSetC1 {
  using parent = CardTableBarrierSetC1;
private:
  // a mark
  constexpr static int decorator_base_sz_ {6};
  // ['10, 13, 18, 29', '6, 13, 18, 29', '10, 13, 17, 18, 29', '6, 13, 18, 20, 29']
  // [537142272, 537141312, 537273344, 538189888]
  // 805577728: atomic cas. 10 ,13, 18, 28, 29
  // 2199828833280ULL: atomic add at.
  const DecoratorSet decorators_[decorator_base_sz_] = {537141312ULL,538189888ULL, 537142272ULL, 537273344ULL, 805577728ULL, 2199828833280ULL};
  const int runtime_stubs_sz_ {256};
  address runtime_stubs[256];

  int get_runtime_stub_index(DecoratorSet decorators, BasicType type) {
    assert(((decorators & (decorators_[0])) == decorators_[0]) || 
           ((decorators & (decorators_[1])) == decorators_[1]) || 
           ((decorators & (decorators_[2])) == decorators_[2]) || 
           ((decorators & (decorators_[3])) == decorators_[3]), " unknown decorator base");
    //    base               |  type
    //    log2(base_sz_)     |  4    
    //    8
    int idx = type - T_BOOLEAN;
    
    // TODO: clear our decorators

    if (decorators == decorators_[5]) {
      return idx | (5 << 4);
    }
    if (decorators == decorators_[4]) {
      return idx | (4 << 4);
    }
    for (int i = 0; i < 4; i++) {
      if ((decorators & (decorators_[i])) == decorators_[i]) {
        return idx | (i << 4);
      }
    }
    assert(false, "should not reach here");
    return -1;
  }

protected:

  virtual void store_at_resolved(LIRAccess& access, LIR_Opr value);

  virtual LIR_Opr atomic_cmpxchg_at_resolved(LIRAccess& access, LIRItem& cmp_value, LIRItem& new_value);

  virtual LIR_Opr atomic_xchg_at_resolved(LIRAccess& access, LIRItem& value);

  virtual LIR_Opr atomic_add_at_resolved(LIRAccess& access, LIRItem& value);
  
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
  address get_runtime_stub(DecoratorSet decorators, BasicType type, bool needs_sync, bool is_volatile, bool is_atomic) {
    if (needs_sync) {
      
      assert(is_reference_type(type), "only reference type may need sync");

      if (is_atomic) {
        return Runtime1::entry_for(Runtime1::lagged_synchronization_atomic_cas_id);
      }
      return is_volatile ? Runtime1::entry_for(Runtime1::lagged_synchronization_volatile_id) : Runtime1::entry_for(Runtime1::lagged_synchronization_id);
    }
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

#endif  // OUR_PERSIST