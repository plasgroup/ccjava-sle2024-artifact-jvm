#ifdef OUR_PERSIST

#include "nvm/nvmMacro.hpp"
#include "oops/oop.hpp"
#include "oops/nvmHeader.hpp"

bool nvmHeader::is_null(void* _fwd) {
  return _fwd == NULL;
}

bool nvmHeader::is_busy(void* _fwd) {
  return _fwd == OURPERSIST_FWD_BUSY;
}

bool nvmHeader::is_fwd(void* _fwd) {
  if (is_null(_fwd) || is_busy(_fwd)) {
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

#endif // OUR_PERSIST
