#ifdef OUR_PERSIST

#ifndef SHARE_OOPS_NVMHEADER_HPP
#define SHARE_OOPS_NVMHEADER_HPP

#include "nvm/nvmMacro.hpp"
#include "oops/oopsHierarchy.hpp"
#include "nvm/oops/nvmOop.hpp"

//
// Bit-format of an object header (most significant first, big endian layout below):
//
//  64 bits:
//  --------
//  NULL:61  unused:3         (volatile object)
//  fwd:61   unused:2  lock:1 (persistent object)
//

class nvmHeader {
 private:
  uintptr_t _value;

 public:
  explicit nvmHeader(uintptr_t value) : _value(value) {}

  nvmHeader() { /* uninitialized */ }

  // It is critical for performance that this class be trivially
  // destructable, copyable, and assignable.

  static nvmHeader from_pointer(void* ptr) {
    return nvmHeader((uintptr_t)ptr);
  }
  void* to_pointer() const {
    return (void*)_value;
  }

  bool operator==(const nvmHeader& other) const {
    return _value == other._value;
  }
  bool operator!=(const nvmHeader& other) const {
    return !operator==(other);
  }

  // Conversion
  uintptr_t value() const { return _value; }

  // Constants
  static const int fwd_bits        = 61;
  static const int unused_gap_bits = 2;
  static const int lock_bits       = 1;

  static const int lock_shift = 0;
  static const int unused_gap_shift    = lock_shift + lock_bits;
  static const int fwd_shift           = unused_gap_shift + unused_gap_bits;

  static const uintptr_t lock_mask                = right_n_bits(lock_bits);
  static const uintptr_t lock_mask_in_place       = lock_mask << lock_shift;
  static const uintptr_t unused_gap_mask          = right_n_bits(unused_gap_bits);
  static const uintptr_t unused_gap_mask_in_place = unused_gap_mask << unused_gap_shift;
  static const uintptr_t fwd_mask                 = right_n_bits(fwd_bits);
  static const uintptr_t fwd_mask_in_place        = fwd_mask << fwd_shift;

  static const uintptr_t flags_mask_in_place         = lock_mask_in_place |
                                                       unused_gap_mask_in_place;

  static nvmHeader zero() { return nvmHeader(uintptr_t(0)); }

  nvmOop fwd() {
    uintptr_t _fwd = _value & fwd_mask_in_place;
    return (nvmOop)_fwd;
  }

  uintptr_t flags() {
    uintptr_t _flags = _value & flags_mask_in_place;
    return _flags;
  }

  bool recoverable() {
    nvmOop nvm_obj = fwd();
    if (nvm_obj == NULL) {
      return false;
    }
    return nvm_obj->responsible_thread() == NULL;
  }

  // Checker
  static bool is_null(nvmOop _fwd);
  static bool is_fwd(nvmOop _fwd);
  bool is_locked() {
    return (flags() & lock_mask) != 0;
  }

  // Setter
  // WARNING: All setters are static functions.
 private:
  static nvmHeader lock_unlock_base(oop obj, uintptr_t mask, bool is_lock);
  static nvmOop cas_fwd(oop obj, nvmOop compare_fwd, nvmOop after_fwd);
 public:
  inline static void set_header(HeapWord* mem, nvmHeader h);
  inline static void set_header(oop obj, nvmHeader h);
  // NOTE: for nvmCardTableBarrierSet.hpp
  static void set_header_no_inline(oop obj, nvmHeader h);
  inline static void set_fwd(oop obj, nvmOop ptr);

  static bool cas_fwd(oop obj, nvmOop after_fwd);
  // NOTE: unused
  static bool cas_fwd_and_lock_when_swapped(oop obj, nvmOop after_fwd);

  static nvmHeader lock(oop obj);
  static void unlock(oop obj);
};

#endif // SHARE_OOPS_NVMHEADER_HPP

#endif // OUR_PERSIST
