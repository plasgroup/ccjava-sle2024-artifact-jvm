#ifdef OUR_PERSIST

#ifndef SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_INLINE_HPP
#define SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_INLINE_HPP

#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "nvm/ourPersist.inline.hpp"

// TODO: rename
// return is_set_durableroot_annotation
inline bool NVMCardTableBarrierSet::static_object_etc(oop obj, ptrdiff_t offset, oop value) {
  assert(obj != NULL, "");

  if (!OurPersist::is_static_field(obj, offset)) {
    return false;
  }

  if (!OurPersist::is_set_durableroot_annotation(obj, offset)) {
    return false;
  }

  if (value != NULL) {
    OurPersist::ensure_recoverable(value);
  }
  return true;
}

#endif // SHARE_GC_NVMCARD_NVMCARDTABLEBARRIERSET_INLINE_HPP

#endif // OUR_PERSIST
