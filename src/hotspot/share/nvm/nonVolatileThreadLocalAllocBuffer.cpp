// #ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "nvm/nvmAllocator.hpp"

#include <pthread.h>

// void NonVolatileThreadLocalAllocBuffer::initialize() {
//   // Thread* thr = thread();
//   // pthread_mutex_lock(&NVMAllocator::allocate_mtx);
    
//   // pthread_mutex_unlock(&NVMAllocator::allocate_mtx);

  
//   // pthread_mutex_lock(&NVMAllocator::allocate_mtx);
//   // void* nvm_next = NULL;
//   // for (size_t i = 0; i < num_of_nvc_small; i++) {
//   //   thr->nvtlab().nvc[i] = new NonVolatileChunkSmall();
//   //   NonVolatileChunk:: add_standby_for_gc(thr->nvtlab[i].nvc[i], i);
//   // }
//   // for (; i < num_of_nvc_medium; i++) {
//   //   thr->nvtlab().nvc[i] = new NonVolatileChunkMedium();
//   // }
//   // thr->nvtlab().nvc[i] = new NonVolatileChunkLarge(); // expected i == 42
// }

void* NonVolatileThreadLocalAllocBuffer::allocate(size_t _word_size) {
  size_t idx = word_size_to_idx(_word_size);

  if (idx == 40) {
    return NULL; // NVCLarge allocation
  }
  if (nvc[idx] == NULL) {
  //   if (_word_size <= 31) { // TODO: 関数にする
  //     nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  //   } else if (_word_size <= 256) {
  //     nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  // }
  // NVCが割り当てられていない場合は割り当てる
    nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  }
  
  NonVolatileChunkSegregate* chunk = nvc[idx];
  void *ptr = chunk->allocation();

  if (ptr != NULL) {
    return ptr;
  }



  // refill
  nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  chunk = nvc[idx];
  // retry
  ptr = chunk->allocation();

  // for (size_t i = 0; i < chunk->get_max_idx(); i++) {
  //   printf("abit[%lu]: %d\n", i, chunk->get_abit(i));
  // }
  if (ptr == NULL) {


    report_vm_error(__FILE__, __LINE__, "refill failed");
  }

  return ptr;
}

Thread* NonVolatileThreadLocalAllocBuffer::thread() {
  return (Thread *)(((char *)this) + in_bytes(start_offset()) - in_bytes(Thread::nvtlab_start_offset()));
}

size_t NonVolatileThreadLocalAllocBuffer::word_size_to_idx(size_t word_size) {
  // NVCSmall
  if (word_size <= num_of_nvc_small)
    return word_size - 1;
  // NVCMedium
  if (word_size <= 41)
    return 32;
  if (word_size <= 54)
    return 33;
  if (word_size <= 71)
    return 34;
  if (word_size <= 83)
    return 35;
  if (word_size <= 122)
    return 36;
  if (word_size <= 159)
    return 37;
  if (word_size <= 208)
    return 38;
  if (word_size <= 256)
    return 39;
  // NVCLarge
  return 40;
}

size_t NonVolatileThreadLocalAllocBuffer::idx_to_word_size(size_t idx) {
  if (idx <= 31) {
    return idx + 1;
  }
  if (idx == 32)
    return 41;
  if (idx == 33)
    return 54;
  if (idx == 34)
    return 71;
  if (idx == 35)
    return 83;
  if (idx == 36)
    return 122;
  if (idx == 37)
    return 159;
  if (idx == 38)
    return 208;
  if (idx == 39)
    return 256;

  report_vm_error(__FILE__, __LINE__, "Should not reach here.");
  return 0;
}

size_t NonVolatileThreadLocalAllocBuffer::idx_to_minimum_word_size(size_t idx) {
  if (idx <= 31)
  {
    return idx + 1;
  }
  if (idx == 32)
    return 32;
  if (idx == 33)
    return 42;
  if (idx == 34)
    return 55;
  if (idx == 35)
    return 72;
  if (idx == 36)
    return 84;
  if (idx == 37)
    return 123;
  if (idx == 38)
    return 160;
  if (idx == 39)
    return 209;

  report_vm_error(__FILE__, __LINE__, "Should not reach here.");
  return 0;
}

// #endif // OUR_PERSIST
