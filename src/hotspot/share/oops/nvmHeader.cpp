
#ifdef OUR_PERSIST

#include "oops/oop.hpp"
#include "oops/nvmHeader.hpp"

bool nvmHeader::is_fwd(void* _fwd) {
  if (from_pointer(_fwd).flags() != uintptr_t(0)) {
    return false;
  }
  if (oopDesc::is_oop(oop(_fwd))) {
    return false;
  }
  return true;
}

bool nvmHeader::is_fwd_or_null(void* _fwd) {
  if (_fwd == NULL) {
    return true;
  }
  return is_fwd(_fwd);
}

#endif // OUR_PERSIST
