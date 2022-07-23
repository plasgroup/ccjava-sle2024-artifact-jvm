#ifdef OUR_PERSIST

#ifndef NVM_OOPS_NVMOOPDESC_HPP
#define NVM_OOPS_NVMOOPDESC_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/ostream.hpp"
#include "oops/instanceMirrorKlass.inline.hpp"

typedef class nvmOopDesc* nvmOop;
class nvmOopDesc {
 private:
  void* _unused1;
  void* _unused2;
  void* _unused3;
};

#endif // NVM_OOPS_NVMOOPDESC_HPP

#endif // OUR_PERSIST
