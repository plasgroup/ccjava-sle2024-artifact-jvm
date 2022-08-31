#ifdef OUR_PERSIST

#include "nvm/nvmMacro.hpp"
#include "nvm/oops/nvmOop.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/thread.inline.hpp"

bool nvmHeader::is_null(nvmOop _fwd) {
  return _fwd == NULL;
}

bool nvmHeader::is_fwd(nvmOop _fwd) {
  if (is_null(_fwd)) {
    return false;
  }
  if (from_pointer(_fwd).flags() != uintptr_t(0)) {
    return false;
  }
  if (oopDesc::is_oop(oop(_fwd))) {
    return false;
  }
  return true;
}

void nvmHeader::set_header_no_inline(oop obj, nvmHeader h) {
  nvmHeader::set_header(obj, h);
}

bool nvmHeader::cas_fwd(oop obj, nvmOop after_fwd) {
  assert(is_fwd(after_fwd), "");

  uintptr_t* header_addr = (uintptr_t*)obj->nvm_header_addr();

  bool success;
  while (true) {
    uintptr_t _flags = obj->nvm_header().flags();
    uintptr_t compare_val = uintptr_t(NULL) | (_flags & (~lock_mask_in_place));
    uintptr_t after_val   = uintptr_t(after_fwd) | (_flags & (~lock_mask_in_place));

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

bool nvmHeader::cas_fwd_and_lock_when_swapped(oop obj, nvmOop after_fwd) {
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
  nvmHeader result = lock_unlock_base(obj, lock_mask_in_place, true);

#ifdef ASSERT
  Thread* locked_thread = obj->nvm_header_locked_thread();
  assert(locked_thread == NULL, "locked_thread: %p", locked_thread);
  obj->set_nvm_header_locked_thread(Thread::current());
#endif // ASSERT

  return result;
}

void nvmHeader::unlock(oop obj) {
  assert((obj->nvm_header().value() & lock_mask_in_place) != 0, "unlocked.");

#ifdef ASSERT
  Thread* locked_thread = obj->nvm_header_locked_thread();
  assert(locked_thread == Thread::current(), "locked_thread: %p", locked_thread);
  obj->set_nvm_header_locked_thread(NULL);
#endif // ASSERT

  uintptr_t header_val = obj->nvm_header().value() & (~lock_mask_in_place);
  nvmHeader::set_header(obj, nvmHeader(header_val));
}

#endif // OUR_PERSIST
