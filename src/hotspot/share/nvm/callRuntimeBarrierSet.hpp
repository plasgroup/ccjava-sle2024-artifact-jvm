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
  template <DecoratorSet ds, typename T>
  inline static void call_runtime_store_in_heap_at(oopDesc* obj, ptrdiff_t off, T val) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::store_in_heap_at(obj, off, val);
  };

  template <DecoratorSet ds, typename T>
  inline static void call_runtime_store_in_heap(oop obj, T* addr, T value) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::template store_in_heap<T>(obj, addr, value);
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
  inline static void call_runtime_oop_store_in_heap(oop base, oop* addr, oop value) {
    NVMCardTableBarrierSet::AccessBarrier<ds>::limited_oop_store_in_heap(base, addr, value);
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
