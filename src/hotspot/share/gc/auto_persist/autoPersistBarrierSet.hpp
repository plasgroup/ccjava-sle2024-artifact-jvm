#ifdef AUTO_PERSIST

#ifndef SHARE_GC_AUTOPERSIST_AUTOPERSISTTABLEBARRIERSET_HPP
#define SHARE_GC_AUTOPERSIST_AUTOPERSISTTABLEBARRIERSET_HPP

#include "gc/auto_persist/autoPersistBarrierSetAssembler.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "gc/shared/collectedHeap.hpp"
#include <x86intrin.h>

class AutoPersistBarrierSet: public CardTableBarrierSet {
  friend class VMStructs;

 public:
  AutoPersistBarrierSet(CardTable* card_table);

 public:
  template <DecoratorSet decorators, typename BarrierSetT = AutoPersistBarrierSet>
  class AccessBarrier: public BarrierSet::AccessBarrier<decorators, BarrierSetT> {
    // parent barrierset class
    typedef CardTableBarrierSet::AccessBarrier<decorators, BarrierSetT> Parent;
    // raw barrierset class
    typedef BarrierSet::AccessBarrier<decorators, BarrierSetT> Raw;

   public:
    // primitive
    template <typename T>
    static T load_in_heap_at(oop base, ptrdiff_t offset) {
      base->autopersist_nvm_header();
      T result = Parent::template load_in_heap_at<T>(base, offset);
      return result;
    }

    template <typename T>
    static void store_in_heap_at(oop base, ptrdiff_t offset, T value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      Parent::store_in_heap_at(base, offset, value);
    }

    template <typename T>
    static T atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, T new_value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      T result = Parent::atomic_xchg_in_heap_at(base, offset, new_value);
      return result;
    }

    template <typename T>
    static T atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, T compare_value, T new_value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      T result = Parent::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
      return result;
    }

    template <typename T>
    static void arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                  arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                  size_t length) {
      if (src_obj != NULL) src_obj->autopersist_nvm_header();
      if (dst_obj != NULL) dst_obj->autopersist_nvm_header();
      OrderAccess::fence();
      // Store in DRAM.
      Parent::arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                dst_obj, dst_offset_in_bytes, dst_raw,
                                length);
    }

    // oop
    static oop oop_load_in_heap_at(oop base, ptrdiff_t offset) {
      base->autopersist_nvm_header();
      oop result = Parent::oop_load_in_heap_at(base, offset);
      return result;
    }

    static void oop_store_in_heap_at(oop base, ptrdiff_t offset, oop value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      Parent::oop_store_in_heap_at(base, offset, value);
    }

    static oop oop_atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, oop new_value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      oop result = Parent::oop_atomic_xchg_in_heap_at(base, offset, new_value);
      return result;
    }

    static oop oop_atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, oop compare_value, oop new_value) {
      base->autopersist_nvm_header();
      OrderAccess::fence();
      oop result = Parent::oop_atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
      return result;
    }

    template <typename T>
    static bool oop_arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                      arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                      size_t length) {
      if (src_obj != NULL) src_obj->autopersist_nvm_header();
      if (dst_obj != NULL) dst_obj->autopersist_nvm_header();
      OrderAccess::fence();
      bool result = Parent::oop_arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                                  dst_obj, dst_offset_in_bytes, dst_raw,
                                                  length);
      return result;
    }

    // clone
    static void clone_in_heap(oop src, oop dst, size_t size) {
      if (src != NULL) src->autopersist_nvm_header();
      if (dst != NULL) dst->autopersist_nvm_header();
      OrderAccess::fence();
      Parent::clone_in_heap(src, dst, size);
    }
  };
};

template<>
struct BarrierSet::GetName<AutoPersistBarrierSet> {
  static const BarrierSet::Name value = BarrierSet::AutoPersistBarrierSet;
};

template<>
struct BarrierSet::GetType<BarrierSet::AutoPersistBarrierSet> {
  typedef ::AutoPersistBarrierSet type;
};

#endif // SHARE_GC_AUTOPERSIST_AUTOPERSISTTABLEBARRIERSET_HPP

#endif // AUTO_PERSIST
