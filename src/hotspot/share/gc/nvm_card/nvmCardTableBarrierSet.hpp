#ifdef OUR_PERSIST

#ifndef SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_HPP
#define SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_HPP

#include "gc/nvm_card/nvmCardTableBarrierSetAssembler.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "gc/shared/collectedHeap.hpp"
#include "nvm/nvmMacro.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/arrayOop.hpp"
#include "oops/klass.hpp"
#include "oops/oop.hpp"
#include "oops/nvmHeader.hpp"

class NVMCardTableBarrierSet: public CardTableBarrierSet {
  friend class VMStructs;

 public:
  NVMCardTableBarrierSet(CardTable* card_table);

 public:
  template <DecoratorSet decorators, typename BarrierSetT = NVMCardTableBarrierSet>
  class AccessBarrier: public CardTableBarrierSet::AccessBarrier<decorators, BarrierSetT> {
    // parent barrierset class
    using Parent = CardTableBarrierSet::AccessBarrier<decorators, BarrierSetT>;
    // raw barrierset class
    using Raw = BarrierSet::AccessBarrier<decorators, BarrierSetT>;

   public:
    static void oop_store_in_heap_raw(oop base, ptrdiff_t offset, oop value) {
#ifndef OURPERSIST_IGNORE_VOLATILE
      // Volatile
      if (OurPersist::is_volatile(base, offset, decorators)) {
        // Counter
        NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                          base, offset, 1 /* volatile */, 1 /* oop */, 0 /* non-atomic */);)

        nvmHeader::lock(base);

        nvmOop before_fwd = base->nvm_header().fwd();
        if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, true)) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
          oop nvm_val = NULL;
          if (value != NULL && OurPersist::is_target(value->klass())) {
            OurPersist::ensure_recoverable(value);
            nvm_val = oop(value->nvm_header().fwd());
          }
#ifndef NO_WUPD
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
        // Store in DRAM.
        Raw::oop_store_in_heap_at(base, offset, value);

        nvmHeader::unlock(base);
        return;
      }
#endif // !OURPERSIST_IGNORE_VOLATILE

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, 0 /* non-volatile */, 1 /* oop */, 0 /* non-atomic */);)

      // Store in DRAM.
      Raw::oop_store_in_heap_at(base, offset, value);

      if (OurPersist::needs_wupd(base, offset, decorators, true)) {
        OrderAccess::fence();
        nvmOop before_fwd = base->nvm_header().fwd();

        if (before_fwd != NULL) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
          oop nvm_val = NULL;
          if (value != NULL && OurPersist::is_target(value->klass())) {
            OurPersist::ensure_recoverable(value);
            nvm_val = oop(value->nvm_header().fwd());
          }
#ifndef NO_WUPD
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
      }
    }

    // primitive
    template <typename T>
    static T load_in_heap_at(oop base, ptrdiff_t offset) {
      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(0 /* load */,
                        base, offset, -1 /* unknown */, 0 /* primitive */, 0 /* non-atomic */);)

      T result = Parent::template load_in_heap_at<T>(base, offset);
      return result;
    }

    template <typename T>
    static void store_in_heap_at(oop base, ptrdiff_t offset, T value) {
      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Skip
      if (offset == oopDesc::mark_offset_in_bytes()) {
        Parent::store_in_heap_at(base, offset, value);
        return;
      }

#ifndef OURPERSIST_IGNORE_VOLATILE
      // Volatile
      if (OurPersist::is_volatile(base, offset, decorators)) {
        assert(offset != oopDesc::mark_offset_in_bytes(), "");

        // Counter
        NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                          base, offset, 1 /* volatile */, 0 /* primitive */, 0 /* non-atomic */);)

        nvmHeader::lock(base);

        nvmOop before_fwd = base->nvm_header().fwd();
        if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, false)) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
#ifndef NO_WUPD
          Raw::store_in_heap_at(oop(before_fwd), offset, value);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
        // Store in DRAM.
        Parent::store_in_heap_at(base, offset, value);

        nvmHeader::unlock(base);
        return;
      }
#endif // !OURPERSIST_IGNORE_VOLATILE

      // Non volatile

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, 0 /* non-volatile */, 0 /* primitive */, 0 /* non-atomic */);)

      // Store in DRAM.
      Parent::store_in_heap_at(base, offset, value);

      OrderAccess::fence();
      nvmOop nvm_fwd = base->nvm_header().fwd();
      if (nvm_fwd == NULL) {
        // Store only in DRAM.
        return;
      }
      assert(nvmHeader::is_fwd(nvm_fwd), "");

      // Store in NVM.
#ifndef NO_WUPD
      Raw::store_in_heap_at(oop(nvm_fwd), offset, value);
      NVM_WRITEBACK(AccessInternal::field_addr(oop(nvm_fwd), offset));
#endif // NO_WUPD
    }

    template <typename T>
    static void store_in_heap(oop base, T* addr, T value) {
      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Store in DRAM.
      auto a = static_cast<void *>(addr);
      auto b = static_cast<void *>(base);
      
      // printf("%s:{addr = %p, base = %p}\n", __func__, a, b);
      Parent::store_in_heap(addr, value);

      // OrderAccess::fence();
      // nvmOop nvm_fwd = base->nvm_header().fwd();
      // if (nvm_fwd == NULL) {
      //   // Store only in DRAM.
      //   return;
      // }
      // assert(false, "not durable at present");
      // assert(nvmHeader::is_fwd(nvm_fwd), "");

      // Store in NVM.
      // Raw::store_in_heap_at(oop(nvm_fwd), offset, value);
      // NVM_WRITEBACK(AccessInternal::field_addr(oop(nvm_fwd), offset));
    }
    
    static void limited_oop_store_in_heap(oop base, oop* addr, oop value) {
      auto a = static_cast<void *>(addr);
      auto b = static_cast<void *>(base);
      
      // printf("%s:{addr = %p, base = %p}\n", __func__, a, b);
      Parent::template oop_store_in_heap(addr, value);
    }
    
    template <typename T>
    static T atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, T new_value) {
#ifdef OURPERSIST_IGNORE_ATOMIC
      return Parent::atomic_xchg_in_heap_at(base, offset, new_value);
#endif // OURPERSIST_IGNORE_ATOMIC

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, -1 /* unknown */, 0 /* primitive */, 1 /* atomic */);)

      nvmHeader::lock(base);
      T result = Parent::template load_in_heap_at<T>(base, offset);

      nvmOop before_fwd = base->nvm_header().fwd();
      if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, false)) {
        assert(nvmHeader::is_fwd(before_fwd), "");

        // Store in NVM.
#ifndef NO_WUPD
        Raw::store_in_heap_at(oop(before_fwd), offset, new_value);
        NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
      }
      // Store and load in DRAM.
      Parent::store_in_heap_at(base, offset, new_value);

      nvmHeader::unlock(base);
      return result;
    }

    template <typename T>
    static T atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, T compare_value, T new_value) {
#ifdef OURPERSIST_IGNORE_ATOMIC
      return Parent::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
#endif // OURPERSIST_IGNORE_ATOMIC

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Skip
      if (offset == oopDesc::mark_offset_in_bytes()) {
        return Parent::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
      }

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, -1 /* unknown */, 0 /* primitive */, 1 /* atomic */);)

      nvmHeader::lock(base);
      T result = load_in_heap_at<T>(base, offset);

      bool swap = result == compare_value;
      if (swap) {
        nvmOop before_fwd = base->nvm_header().fwd();
        if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, false)) {
          assert(nvmHeader::is_fwd(before_fwd), "");
          // Store in NVM.
#ifndef NO_WUPD
          Raw::store_in_heap_at(oop(before_fwd), offset, new_value);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
        // Store in DRAM.
        Parent::store_in_heap_at(base, offset, new_value);
      }

      nvmHeader::unlock(base);

      return result;
    }

    template <typename T>
    static void arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                  arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                  size_t length) {

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      if (dst_obj != NULL && dst_raw == NULL) {
        // heap to heap or native to heap ?
        assert(src_obj != NULL || !Universe::heap()->is_in(src_raw), "");
        assert(dst_obj != NULL && dst_raw == NULL, "");

        // Store in DRAM.
        Parent::arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                  dst_obj, dst_offset_in_bytes, dst_raw,
                                  length);

        OrderAccess::fence();
        nvmOop dst_nvm_obj = dst_obj->nvm_header().fwd();
        if (dst_nvm_obj != NULL) {
          // Store in NVM.
#ifndef NO_WUPD
          Raw::arraycopy_in_heap(dst_obj,               dst_offset_in_bytes, (T*)NULL,
                                 arrayOop(dst_nvm_obj), dst_offset_in_bytes, (T*)NULL,
                                 length);
          NVM_WRITEBACK_LOOP(AccessInternal::field_addr(oop(dst_nvm_obj), dst_offset_in_bytes), length)
#endif // NO_WUPD
        }
      } else {
        // heap to native or native to native ?
        assert(src_obj != NULL || !Universe::heap()->is_in(src_raw), "");
        assert(dst_obj == NULL && !Universe::heap()->is_in(dst_raw), "");

        // Store in DRAM.
        Parent::arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                  dst_obj, dst_offset_in_bytes, dst_raw,
                                  length);
      }
    }

    // oop
    static oop oop_load_in_heap_at(oop base, ptrdiff_t offset) {
      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(0 /* load */,
                        base, offset, -1/* unknown */, 1 /* oop */, 0 /* non-atomic */);)

      oop result = Parent::oop_load_in_heap_at(base, offset);
      return result;
    }

    static void oop_store_in_heap_at(oop base, ptrdiff_t offset, oop value) {
      // Original
      // Parent::oop_store_in_heap_at(base, offset, value);

      // Avoid allocating nvm during GC.
      if ((decorators & AS_NO_KEEPALIVE) != 0) {
        Parent::oop_store_in_heap_at(base, offset, value);
        return;
      }

#ifndef OURPERSIST_IGNORE_VOLATILE
      // Volatile
      if (OurPersist::is_volatile(base, offset, decorators)) {

        // Counter
        NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                          base, offset, 1 /* volatile */, 1 /* oop */, 0 /* non-atomic */);)
        nvmHeader::lock(base);

        nvmOop before_fwd = base->nvm_header().fwd();
        if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, true)) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
          oop nvm_val = NULL;
          if (value != NULL && OurPersist::is_target(value->klass())) {
            OurPersist::ensure_recoverable(value);
            nvm_val = oop(value->nvm_header().fwd());
          }
#ifndef NO_WUPD
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
        // Store in DRAM.
        Parent::oop_store_in_heap_at(base, offset, value);

        nvmHeader::unlock(base);
        return;
      }
#endif // !OURPERSIST_IGNORE_VOLATILE

      // Non volatile

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, 0 /* non-volatile */, 1 /* oop */, 0 /* non-atomic */);)

      // Store in DRAM.
      Parent::oop_store_in_heap_at(base, offset, value);

      if (OurPersist::needs_wupd(base, offset, decorators, true)) {
        OrderAccess::fence();
        nvmOop before_fwd = base->nvm_header().fwd();

        if (before_fwd != NULL) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
          oop nvm_val = NULL;
          if (value != NULL && OurPersist::is_target(value->klass())) {
            OurPersist::ensure_recoverable(value);
            nvm_val = oop(value->nvm_header().fwd());
          }
#ifndef NO_WUPD
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
      }
    }

    static oop oop_atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, oop new_value) {
#ifdef OURPERSIST_IGNORE_ATOMIC
      return Parent::oop_atomic_xchg_in_heap_at(base, offset, new_value);
#endif // OURPERSIST_IGNORE_ATOMIC

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, -1 /* unknown */, 1 /* oop */, 1 /* atomic */);)

      nvmHeader::lock(base);
      oop result = Parent::oop_load_in_heap_at(base, offset);

      nvmOop before_fwd = base->nvm_header().fwd();

      if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, true)) {
        assert(nvmHeader::is_fwd(before_fwd), "");

        // Store in NVM.
        oop nvm_val = NULL;
        if (new_value != NULL && OurPersist::is_target(new_value->klass())) {
          OurPersist::ensure_recoverable(new_value);
          nvm_val = oop(new_value->nvm_header().fwd());
        }
#ifndef NO_WUPD
        Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
        NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
      }
      // Store in DRAM.
      Parent::oop_store_in_heap_at(base, offset, new_value);

      nvmHeader::unlock(base);
      return result;
    }

    static oop oop_atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, oop compare_value, oop new_value) {
#ifdef OURPERSIST_IGNORE_ATOMIC
      return Parent::oop_atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
#endif // OURPERSIST_IGNORE_ATOMIC

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Counter
      NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_access_runtime(1 /* store */,
                        base, offset, -1 /* unknown */, 1 /* oop */, 1 /* atomic */);)

      nvmHeader::lock(base);
      oop result = oop_load_in_heap_at(base, offset);

      bool swap = result == compare_value;
      if (swap) {
        nvmOop before_fwd = base->nvm_header().fwd();
        if (before_fwd != NULL && OurPersist::needs_wupd(base, offset, decorators, true)) {
          assert(nvmHeader::is_fwd(before_fwd), "");

          // Store in NVM.
          oop nvm_val = NULL;
          if (new_value != NULL && OurPersist::is_target(new_value->klass())) {
            OurPersist::ensure_recoverable(new_value);
            nvm_val = oop(new_value->nvm_header().fwd());
          }
#ifndef NO_WUPD
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
#endif // NO_WUPD
        }
        // Store in DRAM.
        Parent::oop_store_in_heap_at(base, offset, new_value);
      }

      nvmHeader::unlock(base);
      return result;
    }

    template <typename T>
    static bool oop_arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                      arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                      size_t length) {
      // heap to heap
      assert(src_obj != NULL && dst_obj != NULL, "");
      assert(src_raw == NULL && dst_raw == NULL, "");
      assert(src_obj->is_objArray() && dst_obj->is_objArray(), "");
      assert(((size_t)objArrayOopDesc::header_size() * HeapWordSize) <= src_offset_in_bytes, "");
      assert(((size_t)objArrayOopDesc::header_size() * HeapWordSize) <= dst_offset_in_bytes, "");

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      // Store in DRAM.
      bool result = Parent::oop_arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                                  dst_obj, dst_offset_in_bytes, dst_raw,
                                                  length);

      bool needs_wupd = OurPersist::needs_wupd(dst_obj, dst_offset_in_bytes, decorators, true);
      if (needs_wupd) {
        OrderAccess::fence();
        nvmOop before_fwd = dst_obj->nvm_header().fwd();

        if (before_fwd != NULL) {
          int oop_bytes = 8;
          assert(HeapWordSize == oop_bytes && type2size[T_OBJECT] * HeapWordSize == oop_bytes, "");

          // Store in NVM.
          nvmOop dst_nvm_obj = dst_obj->nvm_header().fwd();
          for (size_t offset = 0; offset < length * oop_bytes; offset += oop_bytes) {
            oop val = Raw::oop_load_in_heap_at(dst_obj, dst_offset_in_bytes + offset);
            oop nvm_val = NULL;
            if (val != NULL && OurPersist::is_target(val->klass())) {
              OurPersist::ensure_recoverable(val);
              nvm_val = oop(val->nvm_header().fwd());
            }
#ifndef NO_WUPD
            Raw::store_in_heap_at(oop(dst_nvm_obj), dst_offset_in_bytes + offset, nvm_val);
#endif // NO_WUPD
          }
#ifndef NO_WUPD
          NVM_WRITEBACK_LOOP(AccessInternal::field_addr(oop(dst_nvm_obj), dst_offset_in_bytes), length)
#endif // NO_WUPD
        }
      }

      return result;
    }

    // clone
    static void clone_in_heap(oop src, oop dst, size_t size) {
#ifdef ASSERT
      void* dst_obj_nvm_header = dst->nvm_header().to_pointer();
      assert(dst_obj_nvm_header == NULL, "dst_obj.nvm_header: %p", dst_obj_nvm_header);
#endif

      assert((decorators & AS_NO_KEEPALIVE) == 0, "");

      Parent::clone_in_heap(src, dst, size);
      nvmHeader::set_header_no_inline(dst, nvmHeader::zero());
    }
  };
};

template<>
struct BarrierSet::GetName<NVMCardTableBarrierSet> {
  static const BarrierSet::Name value = BarrierSet::NVMCardTableBarrierSet;
};

template<>
struct BarrierSet::GetType<BarrierSet::NVMCardTableBarrierSet> {
  typedef ::NVMCardTableBarrierSet type;
};

#endif // SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_HPP

#endif // OUR_PERSIST
