#ifdef OUR_PERSIST
#ifdef USE_NVTLAB

#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "nvm/nvmAllocator.hpp"

#include <pthread.h>

ChunkSizeInfo NonVolatileThreadLocalAllocBuffer::csi[segregated_num];
size_t NonVolatileThreadLocalAllocBuffer::wsize_to_index[NonVolatileThreadLocalAllocBuffer::min_size_large];

void NonVolatileThreadLocalAllocBuffer::initialize_wsize_to_index() {
  for (size_t i = 0; i < segregated_num; i++) {
    for (size_t j = csi[i].min_size; j <= csi[i].max_size; j++) {
      NonVolatileThreadLocalAllocBuffer::wsize_to_index[j] = i;
    }
  }
}

void NonVolatileThreadLocalAllocBuffer::initialize_csi() {
  NonVolatileThreadLocalAllocBuffer::csi[0] = ChunkSizeInfo(NonVolatileThreadLocalAllocBuffer::min_size_small, NonVolatileThreadLocalAllocBuffer::min_size_small); // 0
  NonVolatileThreadLocalAllocBuffer::csi[1] =  ChunkSizeInfo(2, 2); // 1
  NonVolatileThreadLocalAllocBuffer::csi[2] =   ChunkSizeInfo(3, 3); //2 
    NonVolatileThreadLocalAllocBuffer::csi[3] = ChunkSizeInfo(4, 4); //3
    NonVolatileThreadLocalAllocBuffer::csi[4] = ChunkSizeInfo(5, 5); //4
    NonVolatileThreadLocalAllocBuffer::csi[5] = ChunkSizeInfo(6, 6); //5
    NonVolatileThreadLocalAllocBuffer::csi[6] = ChunkSizeInfo(7, 7); //6
    NonVolatileThreadLocalAllocBuffer::csi[7] = ChunkSizeInfo(8, 8); //7
    NonVolatileThreadLocalAllocBuffer::csi[8] = ChunkSizeInfo(9, 9); //8
    NonVolatileThreadLocalAllocBuffer::csi[9] = ChunkSizeInfo(10, 10); //9
    NonVolatileThreadLocalAllocBuffer::csi[10] = ChunkSizeInfo(11, 11); //10
    NonVolatileThreadLocalAllocBuffer::csi[11] = ChunkSizeInfo(12, 12); //11
    NonVolatileThreadLocalAllocBuffer::csi[12] = ChunkSizeInfo(13, 13); //12
    NonVolatileThreadLocalAllocBuffer::csi[13] = ChunkSizeInfo(14, 14); //13
    NonVolatileThreadLocalAllocBuffer::csi[14] = ChunkSizeInfo(15, 15); //14
    NonVolatileThreadLocalAllocBuffer::csi[15] = ChunkSizeInfo(16, 16); //15
    NonVolatileThreadLocalAllocBuffer::csi[16] = ChunkSizeInfo(17, 17); //16
    NonVolatileThreadLocalAllocBuffer::csi[17] = ChunkSizeInfo(18, 18); //17
    NonVolatileThreadLocalAllocBuffer::csi[18] = ChunkSizeInfo(19, 19); //18
    NonVolatileThreadLocalAllocBuffer::csi[19] = ChunkSizeInfo(20, 20); //19
    NonVolatileThreadLocalAllocBuffer::csi[20] = ChunkSizeInfo(21, 21); //20
    NonVolatileThreadLocalAllocBuffer::csi[21] = ChunkSizeInfo(22, 22); //21
    NonVolatileThreadLocalAllocBuffer::csi[22] = ChunkSizeInfo(23, 23); //22
    NonVolatileThreadLocalAllocBuffer::csi[23] = ChunkSizeInfo(24, 24); //23
    NonVolatileThreadLocalAllocBuffer::csi[24] = ChunkSizeInfo(25, 25); //24
    NonVolatileThreadLocalAllocBuffer::csi[25] = ChunkSizeInfo(26, 26); //25
    NonVolatileThreadLocalAllocBuffer::csi[26] = ChunkSizeInfo(27, 27); //26
    NonVolatileThreadLocalAllocBuffer::csi[27] = ChunkSizeInfo(28, 28); //27
    NonVolatileThreadLocalAllocBuffer::csi[28] = ChunkSizeInfo(29, 29); //28
    NonVolatileThreadLocalAllocBuffer::csi[29] = ChunkSizeInfo(30, 30); //29  
    NonVolatileThreadLocalAllocBuffer::csi[30] = ChunkSizeInfo(NonVolatileThreadLocalAllocBuffer::max_size_small, NonVolatileThreadLocalAllocBuffer::max_size_small); //30  small
    NonVolatileThreadLocalAllocBuffer::csi[31] = ChunkSizeInfo(NonVolatileThreadLocalAllocBuffer::min_size_medium, 41); //31  medium
    NonVolatileThreadLocalAllocBuffer::csi[32] = ChunkSizeInfo(42, 54); //32
    NonVolatileThreadLocalAllocBuffer::csi[33] = ChunkSizeInfo(55, 71); //33
    NonVolatileThreadLocalAllocBuffer::csi[34] = ChunkSizeInfo(72, 83); //34
    NonVolatileThreadLocalAllocBuffer::csi[35] = ChunkSizeInfo(84, 122); //35
    NonVolatileThreadLocalAllocBuffer::csi[36] = ChunkSizeInfo(123, 159); //36
    NonVolatileThreadLocalAllocBuffer::csi[37] = ChunkSizeInfo(160, 208); //37
    NonVolatileThreadLocalAllocBuffer::csi[38] = ChunkSizeInfo(209, max_size_medium); //38  medium
    NonVolatileThreadLocalAllocBuffer::csi[39] = ChunkSizeInfo(NonVolatileThreadLocalAllocBuffer::min_size_large, NonVolatileThreadLocalAllocBuffer::min_size_large); //39  large
}


void* NonVolatileThreadLocalAllocBuffer::allocate(size_t _word_size) {
  if (!small_or_medium(_word_size)) return NULL;
  size_t idx = word_size_to_idx(_word_size);

  if (nvc[idx] == NULL) {
  // NVCが割り当てられていない場合は割り当てる
    nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  }
  
  NonVolatileChunkSegregate* chunk = nvc[idx];
  void *ptr = chunk->allocation();

  if (ptr != NULL) {
    return ptr;
  }
  // this nvc doesn't have empty slots.
  nvc[idx]->set_is_using(false);
  // nvc[idx]->set_is_full(true);

// If NVMGC is enabled, this thread try to find half-used NVC from ready_for_use[].
#ifdef NVMGC

  // find used nvc (need global lock)
  pthread_mutex_lock(&NonVolatileChunkSegregate::gc_mtx);

  NonVolatileChunkSegregate* used_nvc = NonVolatileChunkSegregate::get_ready_for_use_head(idx);
  if (used_nvc != NULL) {
    NonVolatileChunkSegregate::set_ready_for_use_head(used_nvc->get_next_chunk(), idx);  // removing the nvc from ready_for_use
    used_nvc->set_next_chunk(NULL);
    used_nvc->set_is_using(true);
    nvc[idx] = used_nvc;
    NonVolatileChunkSegregate::add_standby_for_gc_without_lock(used_nvc, idx);
  }
  pthread_mutex_unlock(&NonVolatileChunkSegregate::gc_mtx);
  
  chunk = nvc[idx];
  ptr = chunk->allocation();
  if (ptr != NULL) {
    return ptr;
  }

#endif // NVMGC

  // define new nvc
  nvc[idx] = NonVolatileChunkSegregate::generate_new_nvc(idx);
  chunk = nvc[idx];
  // retry
  ptr = chunk->allocation();


  if (ptr == NULL) {
    report_vm_error(__FILE__, __LINE__, "refill failed");
  }

  return ptr;
}

Thread* NonVolatileThreadLocalAllocBuffer::thread() {
  return (Thread *)(((char *)this) + in_bytes(start_offset()) - in_bytes(Thread::nvtlab_start_offset()));
}

size_t NonVolatileThreadLocalAllocBuffer::word_size_to_idx(size_t word_size) {
  if (word_size > min_size_large) {
    word_size = min_size_large;
  }
  return NonVolatileThreadLocalAllocBuffer::wsize_to_index[word_size];
}

size_t NonVolatileThreadLocalAllocBuffer::idx_to_word_size(size_t idx) {
  if (idx < num_of_nvc_small_medium)
    return NonVolatileThreadLocalAllocBuffer::csi[idx].max_size;
  report_vm_error(__FILE__, __LINE__, "idx too large in idx_to_word_size");
  return 0;
}

size_t NonVolatileThreadLocalAllocBuffer::idx_to_minimum_word_size(size_t idx) {
  if (idx < num_of_nvc_small_medium)
    return NonVolatileThreadLocalAllocBuffer::csi[idx].min_size;
  report_vm_error(__FILE__, __LINE__, "idx too large in idx_to_minimum_word_size");
  return 0;
}

#endif // USE_NVTLAB
#endif // OUR_PERSIST
