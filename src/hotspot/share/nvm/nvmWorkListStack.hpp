#ifdef OUR_PERSIST

#ifndef NVM_NVMWORKLISTSTACK_HPP
#define NVM_NVMWORKLISTSTACK_HPP

#include "memory/allocation.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/oopsHierarchy.hpp"

class NVMWorkListStack: public GrowableArray<oop> {
 public:
  NVMWorkListStack(): GrowableArray<oop>(0, mtThread) {
    assert(length() == 0, "sanity check");
  }

  // see push(), pop(), and length() in the parent class
  
  void add(const oop& v) { // for backward compatibility
    assert(v != nullptr, "sanity check");
    push(v);
  }
};

#endif // NVM_NVMWORKLISTSTACK_HPP

#endif // OUR_PERSIST
