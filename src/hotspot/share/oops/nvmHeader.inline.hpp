#ifdef OUR_PERSIST

#ifndef SHARE_OOPS_NVMHEADER_INLINE_HPP
#define SHARE_OOPS_NVMHEADER_INLINE_HPP

#include "oops/oop.inline.hpp"
#include "oops/nvmHeader.hpp"

void nvmHeader::set_header(HeapWord* mem, nvmHeader h) {
  *(nvmHeader*)(((char*)mem) + oopDesc::nvm_header_offset_in_bytes()) = h;
}

void nvmHeader::set_header(oop obj, nvmHeader h) {
  *obj->nvm_header_addr() = h;
}

void* nvmHeader::cas_fwd(oop obj, void* compare_fwd, void* after_fwd) {
  assert(is_null(compare_fwd), "");
  assert(is_busy(after_fwd) || is_fwd(after_fwd), "");

  uintptr_t* header_addr = (uintptr_t*)obj->nvm_header_addr();

  void* before_fwd;
  while (true) {
    uintptr_t _flags = obj->nvm_header().flags();
    uintptr_t compare_val = uintptr_t(compare_fwd) | _flags;
    uintptr_t after_val   = uintptr_t(after_fwd)   | _flags;

    uintptr_t before_val = Atomic::cmpxchg(header_addr, compare_val, after_val);

    nvmHeader before_header = nvmHeader(before_val);
    uintptr_t before_flags  = before_header.flags();
    if (_flags == before_flags) {
      before_fwd = before_header.fwd();
      break;
    }
  }

  return before_fwd;
}

// for OurPersist::set_responsible_thread
void nvmHeader::set_fwd(oop obj, void* ptr) {
  assert(from_pointer(ptr).flags() == uintptr_t(0), "ptr is not a forwarding pointer.");

  uintptr_t header_val = uintptr_t(ptr) | obj->nvm_header().flags();
  *obj->nvm_header_addr() = nvmHeader(header_val);
}

void nvmHeader::lock_unlock_base(oop obj, uintptr_t mask, bool is_lock) {
  assert(is_lock || (obj->nvm_header().value() & mask) != 0, "unlocked.");

  uintptr_t* header_addr = (uintptr_t*)obj->nvm_header_addr();

  bool success = false;
  while (!success) {
    uintptr_t tmp_header     = obj->nvm_header().value();
    uintptr_t compare_header = is_lock ? (tmp_header & ~mask) : (tmp_header |  mask) ; // is_lock ? 0 : 1
    uintptr_t after_header   = is_lock ? (tmp_header |  mask) : (tmp_header & ~mask) ; // is_lock ? 1 : 0

    // is_lock ? CAS(0 --> 1) : CAS(1 --> 0)
    uintptr_t before_header = Atomic::cmpxchg(header_addr, compare_header, after_header);
    success = compare_header == before_header;
  }
}

void nvmHeader::lock_atomic(oop obj) {
  lock_unlock_base(obj, atomic_lock_mask_in_place, true);
}

void nvmHeader::unlock_atomic(oop obj) {
  lock_unlock_base(obj, atomic_lock_mask_in_place, false);
}

void nvmHeader::lock_volatile(oop obj) {
  lock_unlock_base(obj, volatile_lock_mask_in_place, true);
}

void nvmHeader::unlock_volatile(oop obj) {
  lock_unlock_base(obj, volatile_lock_mask_in_place, false);
}

#endif // SHARE_OOPS_NVMHEADER_INLINE_HPP

#endif // OUR_PERSIST
