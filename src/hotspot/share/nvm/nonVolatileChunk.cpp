// #ifdef OUR_PERSIST
#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "runtime/thread.inline.hpp"
#include "nvm/nvmAllocator.hpp"
#include <pthread.h>

pthread_mutex_t NonVolatileChunkSegregate::gc_mtx = PTHREAD_MUTEX_INITIALIZER;
NonVolatileChunkSegregate* NonVolatileChunkSegregate::standby_for_gc[40];
void* NonVolatileChunkSegregate::nvc_address[3750000] = {NULL};
char fill_memory[4] = "42";

void NonVolatileChunkSegregate::initialize_standby_for_gc() {
  size_t i;
  // printf("start first for\n");
  // fflush(stdout);
    // standby_for_gc[0] = new NonVolatileChunkSmall(NULL, NULL, 0);
  for (i=0; i < NonVolatileThreadLocalAllocBuffer::num_of_nvc_small; i++) {
    // printf("i: %lu\n", i);
    // fflush(stdout);
    // new NonVolatileChunkSmall(NULL, NULL, 100);
    // exit(1);
    standby_for_gc[i] = new NonVolatileChunkSmall(NULL, NULL, 1);
  }
  for (; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    standby_for_gc[i] = new NonVolatileChunkMedium(NULL, NULL, 1, 1);
  }
}

void NonVolatileChunkSegregate::set_nvc_address() {
  if (start == NULL) return;
  // uintptr_t segregated_head = (uintptr_t) NVMAllocator::nvm_head;
  // uintptr_t nvc_head = (uintptr_t) start;
  uintptr_t segregated_head = (uintptr_t) NVMAllocator::nvm_head;
  uintptr_t nvc_head = (uintptr_t) start;

  uintptr_t dif = (nvc_head - segregated_head) / 4096;

  NonVolatileChunkSegregate::nvc_address[dif] = this;
  return;
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
  void *_start = NVMAllocator::segregated_top;
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
  assert(idx < NonVolatileThreadLocalAllocBuffer::segregated_num, "");
  NonVolatileChunkSegregate* nvc_list_head = standby_for_gc[idx];
  nvc->next_chunk = nvc_list_head;
  standby_for_gc[idx] = nvc;
  pthread_mutex_unlock(&gc_mtx);
  // print_standby_for_gc();
  return;
}

void NonVolatileChunkSegregate::print_nvc_info() {
  printf("----------\n");
  printf("start: %p\n", start);
  printf("end  : %p\n", end);
  printf("size_class: %lu\n", size_class);
  size_t abit1 = 0;
  size_t mbit1 = 0;
  for (size_t i = 0; i < max_idx; i++) {
    abit1 += abit[i];
    mbit1 += mbit[i];
  }
  printf("abit 1: %lu\n", abit1);
  printf("     0: %lu\n", max_idx);
  printf("mbit 1: %lu\n", mbit1);
  printf("     0: %lu\n", max_idx);
  printf("----------\n");
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

void NonVolatileChunkSegregate::a_0_to_1(size_t idx)
{
  assert(abit[idx] == 0, "");
      abit[idx] = 1;
  return;
}

void NonVolatileChunkSegregate::a_1_to_0(size_t idx)
{
  assert(abit[idx] == 1, "");
      abit[idx] = 0;
  return;
}

void NonVolatileChunkSegregate::m_0_to_1(size_t idx) {
  assert(mbit[idx] == 0, "");
  mbit[idx] = 1;
  return;
}

void NonVolatileChunkSegregate::m_1_to_0(size_t idx)
{
  assert(mbit[idx] == 1, "");
      mbit[idx] = 0;
  return;
}


void* NonVolatileChunkSegregate::idx_2_address(size_t idx) {
  char* ad = (char*) start;
  ad += (size_class * HeapWordSize * idx);
  return ad;
}

void NonVolatileChunkSegregate::mark_object(void* ptr, size_t obj_word_size) {
  if (obj_word_size >= NonVolatileThreadLocalAllocBuffer::minimum_word_size_of_extra_large) {
    return ; // TODO: large marking
  }
  NonVolatileChunkSegregate* nvc = (NonVolatileChunkSegregate *) NonVolatileChunkSegregate::address_to_nvc(ptr);
  if (nvc->is_nvc_small()) {
    nvc = (NonVolatileChunkSmall*) nvc;
  } else if (nvc->is_nvc_medium()) {
    nvc = (NonVolatileChunkMedium*) nvc;
  } else {
    report_vm_error(__FILE__, __LINE__, "should not reach here.");
  }
  
  size_t idx = nvc->address_to_idx(ptr);

  // nvc->reverse_mbit(idx);
  nvc->m_0_to_1(idx);
  printf("marking %p [%lu]\n", NonVolatileChunkSegregate::address_to_nvc(ptr), idx);

      return;
}

void* NonVolatileChunkSegregate::address_to_nvc(void* ptr) {
  uintptr_t ptr_uint = (uintptr_t) ptr;
  uintptr_t segregated_head = (uintptr_t) NVMAllocator::nvm_head;

  uintptr_t dif = (ptr_uint - segregated_head) / 4096;

  return NonVolatileChunkSegregate::get_nvc_address(dif);

  
}

size_t NonVolatileChunkSegregate::address_to_idx(void* ptr) {
  uintptr_t ptr_uint = (uintptr_t) ptr;
  uintptr_t start_uint = (uintptr_t) start;

  size_t dif = (ptr_uint - start_uint) / (HeapWordSize * size_class);

  return dif;

}

void NonVolatileChunkSegregate::sweep_objects() {
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    // NonVolatileChunkSegregate* nvc = (NonVolatileChunkSegregate*) NonVolatileChunkSegregate::nvc_address[i];
    NonVolatileChunkSegregate* nvc = standby_for_gc[i];
    while (nvc->next_chunk != NULL) {
      for (size_t idx = 0; idx < nvc->get_max_idx(); idx++) {
        if (nvc->get_abit(idx) != true) {
          printf("ignore %p [%lu]\n", nvc->get_start(), idx);
          continue;
        }
        if (nvc->get_mbit(idx) != true) {
          nvc->a_1_to_0(idx);
          // disable sweeped memory
          memset(nvc->idx_2_address(idx), 'u', 4);
          printf("sweep %p [%lu]\n", nvc->get_start(), idx);
          nvc->set_is_full(false);
        } else {
          nvc->m_1_to_0(idx);
          printf("keep %p [%lu]\n", nvc->get_start(), idx);
        }
      }
      nvc->set_allication_top(0);
      nvc = nvc->next_chunk;
    }

  }
}

void* NonVolatileChunkSmall::allocation() {
  for (size_t i = allocation_top; i < max_idx; i++) {
    if (!get_abit(i)) {
      char* ad = (char*) idx_2_address(i);
      // reverse_abit(i);
      a_0_to_1(i);
      allocation_top = i + 1;
      return ad;
    }
  }
  return NULL;
}

void* NonVolatileChunkMedium::allocation() {
  for (size_t i = allocation_top; i < max_idx; i++) {
    if (get_abit(i) == false) {
      char *ad = (char *)idx_2_address(i);
      allocation_top = i + 1;
      a_0_to_1(i);
      return ad;
    }
  }
  return NULL;
}


// #endif OUR_PERSIST