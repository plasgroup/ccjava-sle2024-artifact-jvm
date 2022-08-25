#ifdef OUR_PERSIST

#ifndef SHARE_OOPS_NVMHEADER_INLINE_HPP
#define SHARE_OOPS_NVMHEADER_INLINE_HPP

#include "nvm/oops/nvmOop.hpp"
#include "oops/nvmHeader.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/thread.hpp"

void nvmHeader::set_header(HeapWord* mem, nvmHeader h) {
  *(nvmHeader*)(((char*)mem) + oopDesc::nvm_header_offset_in_bytes()) = h;
}

void nvmHeader::set_header(oop obj, nvmHeader h) {
  *obj->nvm_header_addr() = h;
}

void nvmHeader::set_fwd(oop obj, nvmOop ptr) {
  assert(from_pointer(ptr).flags() == uintptr_t(0), "ptr is not a forwarding pointer.");

  uintptr_t header_val = uintptr_t(ptr) | obj->nvm_header().flags();
  *obj->nvm_header_addr() = nvmHeader(header_val);
}

#endif // SHARE_OOPS_NVMHEADER_INLINE_HPP

#endif // OUR_PERSIST
