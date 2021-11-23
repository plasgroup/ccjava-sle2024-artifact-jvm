#ifdef OUR_PERSIST

#include "nvm/nvmMacro.hpp"
#include "oops/oop.hpp"
#include "oops/nvmHeader.hpp"

bool nvmHeader::is_null(void* _fwd) {
  return _fwd == NULL;
}

#ifdef OURPERSIST_CAS_VERSION
bool nvmHeader::is_busy(void* _fwd) {
  return _fwd == OURPERSIST_FWD_BUSY;
}
#endif // OURPERSIST_CAS_VERSION

bool nvmHeader::is_fwd(void* _fwd) {
  if (is_null(_fwd)) {
    return false;
  }
#ifdef OURPERSIST_CAS_VERSION
  if (is_busy(_fwd)) {
    return false;
  }
#endif // OURPERSIST_CAS_VERSION
  if (from_pointer(_fwd).flags() != uintptr_t(0)) {
    return false;
  }
  if (oopDesc::is_oop(oop(_fwd))) {
    return false;
  }
  return true;
}

#endif // OUR_PERSIST
