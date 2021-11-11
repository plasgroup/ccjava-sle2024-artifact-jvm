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
  static const size_t SEGREGATED_REGION_SIZE_GB = 15;
 private:
  static void* nvm_head;
  static void* segregated_top;
  static void* nvm_tail;
  static void* large_head;
  static void* large_top;
  static pthread_mutex_t allocate_mtx;

  // allocate nvm bigger than 256 word.
  static void* allocate_large(size_t size);

 public:
  static void init();
  // allocate an UNINITILIZED memory block in NVM
  static void* allocate(size_t size);

  static void* allocate_chunksize();
};

#endif // NVM_NVMALLOCATOR_HPP

#endif // OUR_PERSIST
