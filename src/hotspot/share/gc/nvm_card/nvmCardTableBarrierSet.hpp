#ifdef OUR_PERSIST

#ifndef SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_HPP
#define SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_HPP

#include "gc/nvm_card/nvmCardTableBarrierSetAssembler.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "nvm/nvmMacro.hpp"
#include "nvm/ourPersist.hpp"

class NVMCardTableBarrierSet: public CardTableBarrierSet {
  friend class VMStructs;

 public:
  NVMCardTableBarrierSet(CardTable* card_table);

 private:
  // TODO: rename
  // return is_set_durableroot_annotation
  inline static bool static_object_etc(oop obj, ptrdiff_t offset, oop value);

  template <DecoratorSet decorators, typename BarrierSetT = NVMCardTableBarrierSet>
  class AccessBarrier: public BarrierSet::AccessBarrier<decorators, BarrierSetT> {
    // parent barrierset class
    typedef CardTableBarrierSet::AccessBarrier<decorators, BarrierSetT> Parent;
    // raw barrierset class
    typedef BarrierSet::AccessBarrier<decorators, BarrierSetT> Raw;

   public:
    // primitive
    template <typename T>
    static T load_in_heap_at(oop base, ptrdiff_t offset) {
      T result = Parent::template load_in_heap_at<T>(base, offset);
      return result;
    }

    template <typename T>
    static void store_in_heap_at(oop base, ptrdiff_t offset, T value) {
      // Original
      // Parent::store_in_heap_at(base, offset, value);

      // Store in DRAM.
      Parent::store_in_heap_at(base, offset, value);

      // Skip markWord.
      if (offset == oopDesc::mark_offset_in_bytes()) {
        return;
      }

      _mm_mfence();
      void* nvm_fwd = base->nvm_header().fwd();

      if (nvm_fwd == NULL || nvm_fwd == OURPERSIST_FWD_BUSY) {
        // Store only in DRAM.
        return;
      }
      assert(nvmHeader::is_fwd(nvm_fwd), "");

      // Store in NVM.
      Raw::store_in_heap_at(oop(nvm_fwd), offset, value);
      NVM_WRITEBACK(AccessInternal::field_addr(oop(nvm_fwd), offset));
    }

    template <typename T>
    static T atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, T new_value) {
      // Original
      // T result = Parent::atomic_xchg_in_heap_at(base, offset, new_value);

      nvmHeader::lock_atomic(base);
      T result = load_in_heap_at<T>(base, offset);
      Parent::store_in_heap_at(base, offset, new_value);
      _mm_mfence();
      void* nvm_fwd = base->nvm_header().fwd();
      if (nvm_fwd != NULL && nvm_fwd != OURPERSIST_FWD_BUSY) {
        Raw::store_in_heap_at(oop(nvm_fwd), offset, new_value);
        NVM_WRITEBACK(AccessInternal::field_addr(oop(nvm_fwd), offset));
      }
      nvmHeader::unlock_atomic(base);

      return result;
    }

    template <typename T>
    static T atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, T compare_value, T new_value) {
      // Original
      // T result = Parent::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);

      T result;
      if (offset == oopDesc::mark_offset_in_bytes()) {
        result = Parent::atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
      } else {
        nvmHeader::lock_atomic(base);
        result = load_in_heap_at<T>(base, offset);
        bool swap = result == compare_value;
        if (swap) {
          Parent::store_in_heap_at(base, offset, new_value);
          _mm_mfence();
          void* nvm_fwd = base->nvm_header().fwd();
          if (nvm_fwd != NULL && nvm_fwd != OURPERSIST_FWD_BUSY) {
            Raw::store_in_heap_at(oop(nvm_fwd), offset, new_value);
            NVM_WRITEBACK(AccessInternal::field_addr(oop(nvm_fwd), offset));
          }
        }
        nvmHeader::unlock_atomic(base);
      }

      return result;
    }

    template <typename T>
    static void arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                  arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                  size_t length) {
      Parent::arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                dst_obj, dst_offset_in_bytes, dst_raw,
                                length);
    }

    // oop
    static oop oop_load_in_heap_at(oop base, ptrdiff_t offset) {
      oop result = Parent::oop_load_in_heap_at(base, offset);
      return result;
    }

    static void oop_store_in_heap_at(oop base, ptrdiff_t offset, oop value) {
      // Original
      // Parent::oop_store_in_heap_at(base, offset, value);

      // Check annotation
      bool is_set_durableroot_annotation =
        NVMCardTableBarrierSet::static_object_etc(base, offset, value);

RETRY:
      void* before_fwd = nvmHeader::cas_fwd(base, NULL, OURPERSIST_FWD_BUSY);
      bool success = before_fwd == NULL;
      if (success) {
        // Store only in DRAM.
        Parent::oop_store_in_heap_at(base, offset, value);
        nvmHeader::set_fwd(base, NULL);
        return;
      }
      if (before_fwd == OURPERSIST_FWD_BUSY) {
        // busy wait
        goto RETRY;
      }

      assert(nvmHeader::is_fwd(before_fwd), "");
      if (value != NULL) {
        OurPersist::ensure_recoverable(value);
      }
      // Store in DRAM.
      Parent::oop_store_in_heap_at(base, offset, value);
      // Store in NVM.
      Raw::oop_store_in_heap_at(oop(before_fwd), offset, oop(value != NULL ? value->nvm_header().fwd() : NULL));
      NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
    }

    static oop oop_atomic_xchg_in_heap_at(oop base, ptrdiff_t offset, oop new_value) {
      // Original
      // oop result = Parent::oop_atomic_xchg_in_heap_at(base, offset, new_value);

      // Check annotation
      bool is_set_durableroot_annotation =
        NVMCardTableBarrierSet::static_object_etc(base, offset, new_value);

      bool success;
      void* before_fwd;
      while (true) {
        before_fwd = nvmHeader::cas_fwd(base, NULL, OURPERSIST_FWD_BUSY);
        if (before_fwd != OURPERSIST_FWD_BUSY) {
          success = before_fwd == NULL;
          break;
        }
      }
      oop result;
      if (success) {
        result = Parent::oop_atomic_xchg_in_heap_at(base, offset, new_value);
        nvmHeader::set_fwd(base, NULL);
      } else {
        if (new_value != NULL) {
          OurPersist::ensure_recoverable(new_value);
        }
        nvmHeader::lock_atomic(base);
        result = Parent::oop_load_in_heap_at(base, offset);
        Parent::oop_store_in_heap_at(base, offset, new_value);
        oop nvm_val = oop(new_value != NULL ? new_value->nvm_header().fwd() : NULL);
        Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
        NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
        nvmHeader::unlock_atomic(base);
      }

      return result;
    }

    static oop oop_atomic_cmpxchg_in_heap_at(oop base, ptrdiff_t offset, oop compare_value, oop new_value) {
      // Original
      // oop result = Parent::oop_atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);

      // Check annotation
      bool is_set_durableroot_annotation =
        NVMCardTableBarrierSet::static_object_etc(base, offset, new_value);

      bool success;
      void* before_fwd;
      while (true) {
        before_fwd = nvmHeader::cas_fwd(base, NULL, OURPERSIST_FWD_BUSY);
        if (before_fwd != OURPERSIST_FWD_BUSY) {
          success = before_fwd == NULL;
          break;
        }
      }
      oop result;
      if (success) {
        result = Parent::oop_atomic_cmpxchg_in_heap_at(base, offset, compare_value, new_value);
        nvmHeader::set_fwd(base, NULL);
      } else {
        if (new_value != NULL) {
          OurPersist::ensure_recoverable(new_value);
        }
        nvmHeader::lock_atomic(base);
        result = oop_load_in_heap_at(base, offset);
        bool swap = result == compare_value;
        if (swap) {
          Parent::oop_store_in_heap_at(base, offset, new_value);
          oop nvm_val = oop(new_value != NULL ? new_value->nvm_header().fwd() : NULL);
          Raw::oop_store_in_heap_at(oop(before_fwd), offset, nvm_val);
          NVM_WRITEBACK(AccessInternal::field_addr(oop(before_fwd), offset));
        }
        nvmHeader::unlock_atomic(base);
      }

      return result;
    }

    // sizeof for arraycopy_in_heap
    //template <typename T>
    //static size_t sizeof_ac(T* val)     { return sizeof(*val);  }
    //static size_t sizeof_ac(void* val)  { return sizeof(jbyte); }

    template <typename T>
    static bool oop_arraycopy_in_heap(arrayOop src_obj, size_t src_offset_in_bytes, T* src_raw,
                                      arrayOop dst_obj, size_t dst_offset_in_bytes, T* dst_raw,
                                      size_t length) {
      bool result = Parent::oop_arraycopy_in_heap(src_obj, src_offset_in_bytes, src_raw,
                                                  dst_obj, dst_offset_in_bytes, dst_raw,
                                                  length);
      return result;
    }

    // clone
    static void clone_in_heap(oop src, oop dst, size_t size) {
      Parent::clone_in_heap(src, dst, size);
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
