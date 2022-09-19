#ifdef OUR_PERSIST

#ifndef NVM_NVMALLOCATOR_HPP
#define NVM_NVMALLOCATOR_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"

class NonVolatileChunkLarge;

class NVMAllocator : AllStatic {
  friend class NonVolatileThreadLocalAllocBuffer;
  friend class NonVolatileChunkSegregate;
  friend class NonVolatileChunkLarge;
  friend class VM_OurPersistRecoveryDramCopy; // DEBUG:
  friend class VM_OurPersistRecoveryInit;     // DEBUG:
 public:
  static const size_t NVM_CHUNK_BYTE_SIZE = 4096;
  static const size_t SEGREGATED_REGION_SIZE_GB = 10;
 private:
  static const uintptr_t NVM_MAP_ADDR = 0x1 << 24;
 private:
  static void* nvm_head;
  static void* segregated_top;
  static void* nvm_tail;
  static void* large_head;
  static void* large_top;
  static NonVolatileChunkLarge* first_free_nvcl;
  static pthread_mutex_t allocate_mtx;

  // allocate nvm bigger than 256 word.
  static void* allocate_large(size_t size);

 public:
  static void* map_addr() { return (void*)NVM_MAP_ADDR; }

  static void init(
#ifdef USE_NVM
  // NVM
  const char* nvm_path = XSTR(NVM_FILE_PATH),
  const char* nvm_path_for_size = XSTR(NVM_FILE_PATH)
#else  // USE_NVM
  // DRAM
  const char* nvm_path = "/dev/zero",
  const char* nvm_path_for_size = XSTR(NVM_FILE_PATH)
#endif // USE_NVM
  );
  // allocate an UNINITILIZED memory block in NVM
  static void* allocate(size_t _word_size);

  static void* allocate_chunksize();
};

#endif // NVM_NVMALLOCATOR_HPP

#endif // OUR_PERSIST
