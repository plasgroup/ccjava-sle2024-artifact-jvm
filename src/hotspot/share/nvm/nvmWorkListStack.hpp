#ifdef OUR_PERSIST

#ifndef NVM_WORKLISTSTACK_HPP
#define NVM_WORKLISTSTACK_HPP

#include "memory/allocation.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/oopsHierarchy.hpp"

class NVMWorkListStack : public CHeapObj<mtNone> {
 private:
  static const unsigned long _work_list_stack_size = 1000000;

  oop _stack[_work_list_stack_size];
  unsigned long _top;

 public:
  NVMWorkListStack() {
    _top = 0;
  }

 public:
  // getter
  static bool work_list_stack_size() { return _work_list_stack_size; }

  // setter
  inline void add(oop obj) {
    assert(_top != _work_list_stack_size, "Stack overflow.");
    assert(obj != NULL, "");
    _stack[_top] = obj;
    _top++;
  }

  inline oop remove() {
    assert(_top != 0, "");
    _top--;
    return _stack[_top];
  }

  inline bool empty() {
    return _top == 0;
  }
};

#endif // NVM_WORKLISTSTACK_HPP

#endif // OUR_PERSIST
