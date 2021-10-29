// #ifdef OUR_PERSIST
#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "runtime/thread.inline.hpp"
#include "nvm/nvmAllocator.hpp"
#include <pthread.h>

pthread_mutex_t NonVolatileChunkSegregate::gc_mtx = PTHREAD_MUTEX_INITIALIZER;
NonVolatileChunkSegregate* NonVolatileChunkSegregate::standby_for_gc[40];

void NonVolatileChunkSegregate::initialize_standby_for_gc() {
  size_t i;
  printf("start first for\n");
  fflush(stdout);
    // standby_for_gc[0] = new NonVolatileChunkSmall(NULL, NULL, 0);
  for (i=0; i < NonVolatileThreadLocalAllocBuffer::num_of_nvc_small; i++) {
    printf("i: %lu\n", i);
    fflush(stdout);
    new NonVolatileChunkSmall(NULL, NULL, 0);
    // standby_for_gc[i] = new NonVolatileChunkSmall(NULL, NULL, 0);
  }
  for (; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    standby_for_gc[i] = new NonVolatileChunkMedium(NULL, NULL, 0, 0);
  }
}

void NonVolatileChunkSegregate::print_standby_for_gc() {
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    NonVolatileChunkSegregate* cur_nvc = standby_for_gc[i];
    while (cur_nvc->next_chunk != NULL) {
      printf("NVC_ADDRESS: %p to %p, size_class: %lu\n", cur_nvc->start, cur_nvc->end, cur_nvc->size_class);
      fflush(stdout);
      cur_nvc = cur_nvc->next_chunk;
    }
  }
}


NonVolatileChunkSegregate* NonVolatileChunkSegregate::generate_new_nvc(size_t idx)
{
  pthread_mutex_lock(&NVMAllocator::allocate_mtx);
  void *_start = NVMAllocator::nvm_head;
  void *_end = NVMAllocator::allocate_chunksize();
  pthread_mutex_unlock(&NVMAllocator::allocate_mtx);

  if (idx <= NonVolatileThreadLocalAllocBuffer::num_of_nvc_small) {
    NonVolatileChunkSegregate* nvcs = new NonVolatileChunkSmall(_start, _end, NonVolatileThreadLocalAllocBuffer::idx_to_word_size(idx));
    add_standby_for_gc(nvcs, idx);
    return nvcs;
  } else {
    NonVolatileChunkSegregate* nvcm = new NonVolatileChunkMedium(_start, _end, NonVolatileThreadLocalAllocBuffer::idx_to_word_size(idx), NonVolatileThreadLocalAllocBuffer::idx_to_minimum_word_size(idx));
    add_standby_for_gc(nvcm, idx);
    return nvcm;
  }
}

void NonVolatileChunkSegregate::add_standby_for_gc(NonVolatileChunkSegregate* nvc, size_t idx) {
  pthread_mutex_lock(&gc_mtx);

  if (idx >= NonVolatileThreadLocalAllocBuffer::segregated_num) {
    pthread_mutex_unlock(&gc_mtx);
    report_vm_error(__FILE__, __LINE__, "out of range.");
    return;
  }
  NonVolatileChunkSegregate* nvc_list_head = standby_for_gc[idx];
  nvc->next_chunk = nvc_list_head;
  standby_for_gc[idx] = nvc;
  pthread_mutex_unlock(&gc_mtx);
  print_standby_for_gc();
  return;
}


bool NonVolatileChunkSegregate::get_abit(size_t idx) {
  if (idx > max_idx) {
    report_vm_error(__FILE__, __LINE__, "get_abit: index out of range.");
  }
  return abit[idx];  // TODO:1
}

bool NonVolatileChunkSegregate::get_mbit(size_t idx) {
  if (idx > max_idx) {
    report_vm_error(__FILE__, __LINE__, "get_mbit: index out of range.");
  }
  return mbit[idx];  // TODO:1
}

void NonVolatileChunkSegregate::reverse_abit(size_t idx) {
    if (idx > max_idx) {
    report_vm_error(__FILE__, __LINE__, "reverse_abit: index out of range.");
  }
  abit[idx] = !abit[idx];
}

void NonVolatileChunkSegregate::reverse_mbit(size_t idx) {
    if (idx > max_idx) {
    report_vm_error(__FILE__, __LINE__, "get_mbit: index out of range.");
  }
  mbit[idx] = !mbit[idx];
}

void* NonVolatileChunkSegregate::idx_2_address(size_t idx) {
  char* ad = (char*) start;
  ad += (size_class * HeapWordSize * idx);
  return ad;
}

void* NonVolatileChunkSmall::allocation() {
  for (size_t i = 0; i < max_idx; i++) {
    if (get_abit(i) == false) {
      char* ad = (char*) idx_2_address(i);
      reverse_abit(i);
      return ad;
    }
  }
  return NULL;
}

void* NonVolatileChunkMedium::allocation() {
  for (size_t i = 0; i < max_idx; i++) {
    if (get_abit(i) == false) {
      char *ad = (char *)idx_2_address(i);
      reverse_abit(i);
      return ad;
    }
  }
  return NULL;
}


// #endif OUR_PERSIST