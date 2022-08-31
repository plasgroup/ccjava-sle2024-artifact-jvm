#ifdef OUR_PERSIST

#ifndef NVM_NVMALLOCATOR_HPP
#define NVM_NVMALLOCATOR_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"

class NonVolatileChunkLarge;

// DEBUG: This is a temporary class to test the allocator
class nvmMirrorOopDesc;
typedef nvmMirrorOopDesc* nvmMirrorOop;
class NvmMeta {
 public:
  uintptr_t _state_flag;
  nvmMirrorOop _mirrors_head;
  void* _nvm_head;
  static NvmMeta* meta() {
    return (NvmMeta*)0x700000000000;
  }
  uintptr_t* state_flag_addr() {
    return &_state_flag;
  }
  nvmMirrorOop* mirrors_head_addr() {
    return &_mirrors_head;
  }
  void** nvm_head_addr() {
    return &_nvm_head;
  }
};

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
  static void init(
#ifdef USE_NVM
  const char* nvm_path = XSTR(NVM_FILE_PATH) // NVM
#else  // USE_NVM
  const char* nvm_path = "/dev/zero"         // DRAM
#endif // USE_NVM
  );
  // allocate an UNINITILIZED memory block in NVM
  static void* allocate(size_t _word_size);

  static void* allocate_chunksize();
};

#endif // NVM_NVMALLOCATOR_HPP

#endif // OUR_PERSIST
