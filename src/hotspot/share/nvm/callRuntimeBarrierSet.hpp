#ifdef OUR_PERSIST

#ifndef NVM_CALLRUNTIMEBARRIERSET_HPP
#define NVM_CALLRUNTIMEBARRIERSET_HPP

#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/shared/barrierSet.inline.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"

// To call runtime functions from the interpreter.
class CallRuntimeBarrierSet : AllStatic {
  // Get a function pointer.
 public:
  static void* store_in_heap_at_ptr(DecoratorSet decorators, BasicType type);
  static void* load_in_heap_at_ptr(DecoratorSet decorators, BasicType type);
  static void* ensure_recoverable_ptr();
  static void* is_target_ptr();
  static void* needs_wupd_ptr(DecoratorSet decorators, bool is_oop);

  // Type conversion between oopDesc* and oop.
 private:
  template <DecoratorSet ds>
  inline static void c1_call_runtime_oop_store_in_heap(oopDesc* base, oopDesc** addr, oopDesc* value) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::c1_limited_oop_store_in_heap(base, reinterpret_cast<oop*>(addr), value);
  };

  template <DecoratorSet ds, typename T>
  inline static void c1_call_runtime_store_in_heap(oopDesc* base, T* addr, T value) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::template c1_store_in_heap<T>(base, addr, value);
  };

//  template <DecoratorSet ds>
//  inline static int c1_call_runtime_oop_atomic_cmpxchg_in_heap(oopDesc* base, oopDesc**  addr, oopDesc* cmp, oopDesc* value) {
//    int result = NVMCardTableBarrierSet::AccessBarrier<ds>::c1_oop_atomic_cmpxchg_in_heap(base, reinterpret_cast<oop*>(addr), cmp, value);
//    assert(result == 1 || result == 0, "must be");
//    return result;
//  };

  template <DecoratorSet ds, typename T>
  inline static T c1_call_runtime_atomic_add_at_in_heap(oopDesc* base, T* addr, T value) {
    T result = NVMCardTableBarrierSet::AccessBarrier<ds>::template c1_atomic_add_at_in_heap(base, addr, value);
    return result;
  };
  template <DecoratorSet ds, typename T>
  inline static int c1_call_runtime_atomic_cmpxchg_in_heap(oopDesc* base, T* addr, T cmp, T value) {
    int result = NVMCardTableBarrierSet::AccessBarrier<ds>::template c1_atomic_cmpxchg_in_heap<T>(base, addr, cmp, value);
    assert(result == 1 || result == 0, "must be");
    return result;
  };

  template <DecoratorSet ds, typename T>
  inline static void call_runtime_store_in_heap_at(oopDesc* obj, ptrdiff_t off, T val) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::store_in_heap_at(obj, off, val);
  };

  template <DecoratorSet ds, typename T>
  inline static T call_runtime_load_in_heap_at(oopDesc* obj, ptrdiff_t off) {
    T result = NVMCardTableBarrierSet::AccessBarrier<ds>::template load_in_heap_at<T>(obj, off);
    return result;
  };

  template <DecoratorSet ds>
  inline static void call_runtime_oop_store_in_heap_at(oopDesc* obj, ptrdiff_t off, oopDesc* val) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::oop_store_in_heap_at(obj, off, val);
  };

  template <DecoratorSet ds>
  inline static oopDesc* call_runtime_oop_load_in_heap_at(oopDesc* obj, ptrdiff_t off) {
    return NVMCardTableBarrierSet::AccessBarrier<ds>::oop_load_in_heap_at(obj, off);
  };

  inline static void call_runtime_ensure_recoverable(oopDesc* obj) {
    OurPersist::ensure_recoverable(obj);
  };

  inline static bool call_runtime_is_target(oopDesc* obj) {
    return OurPersist::is_target(obj->klass());
  };

  template <DecoratorSet ds, bool is_oop>
  inline static bool call_runtime_needs_wupd(oopDesc* obj, ptrdiff_t off) {
    return OurPersist::needs_wupd(obj, off, ds, is_oop);
  };
};

#endif // NVM_CALLRUNTIMEBARRIERSET_HPP

#endif // OUR_PERSIST
