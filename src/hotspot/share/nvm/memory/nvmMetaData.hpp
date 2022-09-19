#ifdef OUR_PERSIST

#ifndef NVM_MEMORY_NVMMETADATA_HPP
#define NVM_MEMORY_NVMMETADATA_HPP

#include "utilities/globalDefinitions.hpp"
#include "nvm/nvmAllocator.hpp"

class nvmMirrorOopDesc;
typedef nvmMirrorOopDesc* nvmMirrorOop;

class NVMMetaData {
 public:
  uintptr_t _state_flag;
  nvmMirrorOop _mirrors_head;
  void* _nvm_head;

  static NVMMetaData* meta() {
    return (NVMMetaData*)NVMAllocator::map_addr();
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

#endif // NVM_MEMORY_NVMMETADATA_HPP

#endif // OUR_PERSIST
