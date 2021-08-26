
#ifdef OUR_PERSIST

#ifndef SHARE_OOPS_NVMHEADER_HPP
#define SHARE_OOPS_NVMHEADER_HPP

#include "oops/oopsHierarchy.hpp"
#include "runtime/atomic.hpp"

//
// Bit-format of an object header (most significant first, big endian layout below):
//
//  64 bits:
//  --------
//  NULL:61  unused:1   volatile_lock:1   atomic_lock:1 (volatile object)
//  fwd:61   unused:1   volatile_lock:1   atomic_lock:1 (persistent object)
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
  static const int fwd_bits           = 61;
  static const int unused_gap_bits    = 1;
  static const int atomic_lock_bits   = 1;
  static const int volatile_lock_bits = 1;

  static const int volatile_lock_shift = 0;
  static const int atomic_lock_shift   = volatile_lock_shift + volatile_lock_bits;
  static const int unused_gap_shift    = atomic_lock_shift + atomic_lock_bits;
  static const int fwd_shift           = unused_gap_shift + unused_gap_bits;

  static const uintptr_t volatile_lock_mask          = right_n_bits(volatile_lock_bits);
  static const uintptr_t volatile_lock_mask_in_place = volatile_lock_mask << volatile_lock_shift;
  static const uintptr_t atomic_lock_mask            = right_n_bits(atomic_lock_bits);
  static const uintptr_t atomic_lock_mask_in_place   = atomic_lock_mask << atomic_lock_shift;
  static const uintptr_t unused_gap_mask             = right_n_bits(unused_gap_bits);
  static const uintptr_t unused_gap_mask_in_place    = unused_gap_mask << unused_gap_shift;
  static const uintptr_t fwd_mask                    = right_n_bits(fwd_bits);
  static const uintptr_t fwd_mask_in_place           = fwd_mask << fwd_shift;

  static const uintptr_t flags_mask_in_place         = volatile_lock_mask_in_place |
                                                       atomic_lock_mask_in_place |
                                                       unused_gap_mask_in_place;

  static nvmHeader zero() { return nvmHeader(uintptr_t(0)); }

  void* fwd() {
    uintptr_t _fwd = _value & fwd_mask_in_place;
    return (void*)_fwd;
  }

  uintptr_t flags() {
    uintptr_t _flags = _value & flags_mask_in_place;
    return _flags;
  }

  bool recoverable() {
    void* nvm_obj = fwd();
    return false; // TODO:
    //return nvm_obj != NULL && NVM::responsible_thread(nvm_obj) == NULL;
  }

  // Checker
  static bool is_fwd(void* _fwd);
  static bool is_fwd_or_null(void* _fwd);

  // Setter
  // WARNING: All setters are static functions.
private:
  inline static void lock_unlock_base(oop obj, uintptr_t mask, bool is_lock);
public:
  inline static void set_header(HeapWord* mem, nvmHeader h);
  inline static void set_header(oop obj, nvmHeader h);
  inline static void* cas_fwd(oop obj, void* compare_fwd, void* after_fwd);
  inline static void set_fwd(oop obj, void* ptr);
  inline static void lock_atomic(oop obj);
  inline static void unlock_atomic(oop obj);
  inline static void lock_volatile(oop obj);
  inline static void unlock_volatile(oop obj);
};

#endif // SHARE_OOPS_NVMHEADER_HPP

#endif // OUR_PERSIST
