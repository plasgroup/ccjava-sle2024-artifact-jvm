#ifdef OUR_PERSIST

#ifndef NVM_NVMALLOCATOR_HPP
#define NVM_NVMALLOCATOR_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"

class NVMAllocator : AllStatic {
 private:
  static void* nvm_head;
  static void* nvm_tail;
  static pthread_mutex_t allocate_mtx;

 public:
  static void init();
  // allocate an UNINITILIZED memory block in NVM
  static void* allocate(size_t size);
};

#endif // NVM_NVMALLOCATOR_HPP

#endif // OUR_PERSIST
