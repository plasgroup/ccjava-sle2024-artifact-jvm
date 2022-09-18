#ifdef OUR_PERSIST

#include "nvm/oops/nvmOop.hpp"
#include "oops/oop.inline.hpp"

#ifdef ASSERT
  int nvmOopDesc::_static_verify_count = 0;
#endif // ASSERT

void nvmOopDesc::static_verify() {
#ifdef ASSERT
  _static_verify_count++;
  assert(_static_verify_count < 10, "Too many checks.");
#endif // ASSERT
  if (sizeof(nvmOopDesc) != oopDesc::header_size() * HeapWordSize) {
    report_vm_error(__FILE__, __LINE__, "sizeof(nvmOopDesc) != oopDesc::header_size()");
  }
}

#endif // OUR_PERSIST
