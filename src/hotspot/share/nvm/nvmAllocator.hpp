#ifdef OUR_PERSIST

#ifndef NVM_NVMALLOCATOR_HPP
#define NVM_NVMALLOCATOR_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"

class NVMAllocator : AllStatic {
  friend class NonVolatileThreadLocalAllocBuffer;
  friend class NonVolatileChunkSegregate;
public:
  static const size_t NVM_CHUNK_BYTE_SIZE = 4096;
 private:
  static void* nvm_head;
  static void* nvm_tail;
  static pthread_mutex_t allocate_mtx;

 public:
  static void init();
  // allocate an UNINITILIZED memory block in NVM
  static void* allocate(size_t size);

  static void* allocate_chunksize();
};

#endif // NVM_NVMALLOCATOR_HPP

#endif // OUR_PERSIST
