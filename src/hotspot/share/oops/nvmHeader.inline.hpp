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

bool nvmHeader::cas_fwd_and_lock_when_swapped(oop obj, void* after_fwd) {
  assert(is_fwd(after_fwd), "");

  uintptr_t* header_addr = (uintptr_t*)obj->nvm_header_addr();

  bool success;
  while (true) {
    uintptr_t _flags = obj->nvm_header().flags();
    uintptr_t compare_val = uintptr_t(NULL) | (_flags & (~lock_mask_in_place));
    uintptr_t after_val   = uintptr_t(after_fwd) | _flags | lock_mask_in_place;

    uintptr_t before_val = Atomic::cmpxchg(header_addr, compare_val, after_val);

    if (nvmHeader(compare_val).fwd() != nvmHeader(before_val).fwd()) {
      success = false;
      break;
    }

    if (nvmHeader(compare_val).flags() == nvmHeader(before_val).flags()) {
      // fwd1 == fwd2 && flag1 == flag2 <--> val1 == val2 <--> true
      success = true;
      break;
    }
  }

  return success;
}

// for OurPersist::set_responsible_thread
void nvmHeader::set_fwd(oop obj, void* ptr) {
  assert(from_pointer(ptr).flags() == uintptr_t(0), "ptr is not a forwarding pointer.");

  uintptr_t header_val = uintptr_t(ptr) | obj->nvm_header().flags();
  *obj->nvm_header_addr() = nvmHeader(header_val);
}

nvmHeader nvmHeader::lock_unlock_base(oop obj, uintptr_t mask, bool is_lock) {
  assert(is_lock || (obj->nvm_header().value() & mask) != 0, "unlocked.");

  uintptr_t* header_addr = (uintptr_t*)obj->nvm_header_addr();

  uintptr_t before_header;
  bool success = false;
  while (!success) {
    uintptr_t tmp_header     = obj->nvm_header().value();
    uintptr_t compare_header = is_lock ? (tmp_header & ~mask) : (tmp_header |  mask) ; // is_lock ? 0 : 1
    uintptr_t after_header   = is_lock ? (tmp_header |  mask) : (tmp_header & ~mask) ; // is_lock ? 1 : 0

    // is_lock ? CAS(0 --> 1) : CAS(1 --> 0)
    before_header = Atomic::cmpxchg(header_addr, compare_header, after_header);
    success = compare_header == before_header;
  }

  return nvmHeader(before_header);
}

nvmHeader nvmHeader::lock(oop obj) {
  return lock_unlock_base(obj, lock_mask_in_place, true);
}

void nvmHeader::unlock(oop obj) {
  assert((obj->nvm_header().value() & lock_mask_in_place) != 0, "unlocked.");

  uintptr_t header_val = obj->nvm_header().value() & (~lock_mask_in_place);
  nvmHeader::set_header(obj, nvmHeader(header_val));
}

#endif // SHARE_OOPS_NVMHEADER_INLINE_HPP

#endif // OUR_PERSIST
