#ifdef OUR_PERSIST
#ifdef USE_NVTLAB

#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "runtime/thread.inline.hpp"
#include "nvm/nvmAllocator.hpp"
#include <pthread.h>

pthread_mutex_t NonVolatileChunkSegregate::gc_mtx = PTHREAD_MUTEX_INITIALIZER;
NonVolatileChunkSegregate* NonVolatileChunkSegregate::standby_for_gc[40];

void* NonVolatileChunkSegregate::nvc_address[3750000] = {NULL};
bool NonVolatileChunkLarge::has_available_chunk = false;

#ifdef USE_NVTLAB
#ifdef NVMGC
NonVolatileChunkSegregate* NonVolatileChunkSegregate::ready_for_use[40];
const float NonVolatileChunkSegregate::ready_for_use_threshold = 0.1;
#endif // NVMGC
#endif // USE_NVTLAB

void NonVolatileChunkSegregate::initialize_standby_for_gc() {
  size_t i;
    for (i=0; i < NonVolatileThreadLocalAllocBuffer::num_of_nvc_small; i++) {
    standby_for_gc[i] = NULL;
  }
  for (; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    standby_for_gc[i] = NULL;
  }
}

void NonVolatileChunkSegregate::set_nvc_address() {
  if (start == NULL) return;
  uintptr_t segregated_head = (uintptr_t) NVMAllocator::nvm_head;
  uintptr_t nvc_head = (uintptr_t) start;

  uintptr_t dif = (nvc_head - segregated_head) / 4096;

  NonVolatileChunkSegregate::nvc_address[dif] = this;
  return;
}

void NonVolatileChunkSegregate::print_standby_for_gc() {
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    NonVolatileChunkSegregate* cur_nvc = standby_for_gc[i];
    while (cur_nvc != NULL) {
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

void NonVolatileChunkSegregate::add_standby_for_gc_without_lock(NonVolatileChunkSegregate* nvc, size_t idx) {
  assert(idx < NonVolatileThreadLocalAllocBuffer::segregated_num, "");
  NonVolatileChunkSegregate* nvc_list_head = standby_for_gc[idx];
  nvc->next_chunk = nvc_list_head;
  standby_for_gc[idx] = nvc;
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
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool ret = (abit[base] & mask) != 0;
  return ret;
}



void NonVolatileChunkSegregate::a_0_to_1(size_t idx) {
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (abit[base] & mask) != 0;
  assert(b == false, "");
  abit[base] = abit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::a_1_to_0(size_t idx) {
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (abit[base] & mask) != 0;
  assert(b == true, "");
  abit[base] = abit[base] ^ mask;
  return;
}





void* NonVolatileChunkSegregate::idx_2_address(size_t idx) {
  char* ad = (char*) start;
  ad += (size_class * HeapWordSize * idx);
  return ad;
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



void* NonVolatileChunkSmall::allocation() {
  for (size_t i = allocation_top; i < max_idx; i++) {
    if (!get_abit(i)) {
      char* ad = (char*) idx_2_address(i);
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


void* NonVolatileChunkLarge::allocation(size_t word_size) {
  void* ptr = NULL;
  pthread_mutex_lock(&NVMAllocator::allocate_mtx);  // TODO: Large allocation専用のlockを作るほうが高速
  if (NVMAllocator::nvm_head == NULL) {
    NVMAllocator::init();
    }

  NonVolatileChunkLarge* nvcl = NVMAllocator::first_free_nvcl;
  NonVolatileChunkLarge* left_nvcl = (NonVolatileChunkLarge*) NULL;

  while (true) {
    size_t nvcl_size = nvcl->get_word_size();
    static_assert(sizeof(NonVolatileChunkLarge) % HeapWordSize == 0);
    if (nvcl_size >= word_size + sizeof(NonVolatileChunkLarge) || !nvcl->get_alloc()) {
      // going to use this nvcl.
      // if as large as needed size, just use.
      // if too large, cut from tail.
      if (nvcl_size > word_size + ((sizeof(NonVolatileChunkLarge)-1) / HeapWordSize) + 1 + NonVolatileChunkLarge::MINIMUM_WORD_SIZE_OF_NVCLARGE_ALLOCATION) {
        // cut from tail
        size_t need_byte_size = word_size * HeapWordSize + sizeof(NonVolatileChunkLarge);
        void* new_nvcl_header = (void*) (((char*) nvcl->get_dummy() + (nvcl_size * HeapWordSize - need_byte_size)));
        NonVolatileChunkLarge* new_nvcl = new(new_nvcl_header) NonVolatileChunkLarge(true, word_size, nvcl->get_next_chunk());
        // nvcl->set_next_chunk(new_nvcl);
        nvcl->set_word_size(nvcl_size - (((need_byte_size - 1) / HeapWordSize) + 1) );
        ptr = (void*) &(new_nvcl->dummy);
        break;
      } else {
        // use the nvcl
        if (nvcl == NVMAllocator::first_free_nvcl) {
          NVMAllocator::first_free_nvcl = nvcl->get_next_chunk();
        } else {
          left_nvcl->set_next_chunk(nvcl->get_next_chunk());
        }
        nvcl->next_chunk = (NonVolatileChunkLarge*) NULL;
        nvcl->a_0_to_1();
        ptr = (void*) &(nvcl->dummy);
        break;
      }
    }
    // try next chunk or break the loop
    if (nvcl->get_next_chunk() == NULL) {
      // reach the tail chunk and cannot allocate
      break;
    }
    left_nvcl = nvcl;
    nvcl = left_nvcl->get_next_chunk();
  }

  pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
  return ptr;
}







void NonVolatileChunkLarge::merge(NonVolatileChunkLarge* nvcl) {
  size_t merged_this_byte_size = sizeof(NonVolatileChunkLarge) + ((word_size + nvcl->get_word_size()) * HeapWordSize);
  assert(merged_this_byte_size % HeapWordSize == 0, "");
  word_size = merged_this_byte_size / HeapWordSize;
}

void NonVolatileChunkLarge::print_free_list() {
  NonVolatileChunkLarge* nvcl = NVMAllocator::first_free_nvcl;
  while (nvcl != NULL) {
    printf("NVCLarge %p: size %lu next %p\n", nvcl, nvcl->get_word_size(), nvcl->get_next_chunk());
    nvcl = nvcl->get_next_chunk();
  }
  printf("nvm_tail: %p\n", NVMAllocator::nvm_tail);
}

void NonVolatileChunkLarge::a_0_to_1() {
    assert(alloc == false, "");
    alloc = true;
}

void NonVolatileChunkLarge::a_1_to_0() {
    assert(alloc == true, "");
    alloc = false;
}




#ifdef NVMGC


bool NonVolatileChunkSegregate::get_mbit(size_t idx) {
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool ret = (mbit[base] & mask) != 0;
  return ret;
}


void NonVolatileChunkSegregate::m_0_to_1(size_t idx) {
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (mbit[base] & mask) != 0;
  assert(b == false, "");
  mbit[base] = mbit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::m_1_to_0(size_t idx) {
  assert(idx <= max_idx, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (mbit[base] & mask) != 0;
  assert(b == true, "");
  mbit[base] = mbit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::mark_object(void* ptr, size_t obj_word_size) {
  if (obj_word_size >= NonVolatileChunkLarge::MINIMUM_WORD_SIZE_OF_NVCLARGE_ALLOCATION) {
    return;
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
  nvc->m_0_to_1(idx);
  return;
}

void NonVolatileChunkSegregate::sweep_objects() {
  size_t count = 0;
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    NonVolatileChunkSegregate* nvc = standby_for_gc[i];
    while (nvc != NULL) {
      count = 0;
      for (size_t idx = 0; idx < nvc->get_max_idx(); idx++) {
        if (nvc->get_abit(idx) != true) {
          // empty slot
          count += 1;
          continue;
        }
        if (nvc->get_mbit(idx) != true) {
          // marked non-empty slot
          nvc->a_1_to_0(idx);
          // memset(nvc->idx_2_address(idx), 'u', HeapWordSize * nvc->get_size_class());
        } else {
          // unmarked non-empty slot -> sweep
          count += 1;
          nvc->m_1_to_0(idx);
        }
      }
      nvc->set_num_of_empty_slots(count);
      nvc->set_allication_top(0);
      nvc = nvc->next_chunk;
    }
  }


  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    NonVolatileChunkSegregate* nvc = ready_for_use[i];
    while (nvc != NULL) {
      count = 0;
      for (size_t idx = 0; idx < nvc->get_max_idx(); idx++) {
        if (nvc->get_abit(idx) != true) {
          continue;
        }
        if (nvc->get_mbit(idx) != true) {
          nvc->a_1_to_0(idx);
        } else {
          count += 1;
          nvc->m_1_to_0(idx);
        }
      }
      nvc->set_num_of_empty_slots(count);
      nvc->set_allication_top(0);
      nvc = nvc->next_chunk;
    }
  }
}

void NonVolatileChunkSegregate::update_ready_for_use() {
  // find nvc which has lots of empty slots and move it to "ready_for_use" only when it isn't used by other threads.
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    NonVolatileChunkSegregate* nvc = standby_for_gc[i];
    while (nvc != NULL) {
      if (nvc->get_is_using() == false && nvc->get_next_chunk() != NULL) {
        NonVolatileChunkSegregate* next_chunk = nvc->get_next_chunk();  // not NULL
        NonVolatileChunkSegregate* next_next_chunk = next_chunk->get_next_chunk(); // may be NULL
        float next_nvc_free_slots_percent = (float)next_chunk->get_num_of_empty_slots() / (float)next_chunk->get_max_idx() * 100;
        if (next_nvc_free_slots_percent > NonVolatileChunkSegregate::ready_for_use_threshold) {
          NonVolatileChunkSegregate::move_from_standby_to_ready(next_chunk, i);
          nvc->set_next_chunk(next_next_chunk);
        }
      }
      nvc = nvc->get_next_chunk();
    }
  }
}

void NonVolatileChunkSegregate::move_from_standby_to_ready(NonVolatileChunkSegregate* nvc, size_t list_idx) {
  size_t num_of_empty_slots;
  NonVolatileChunkSegregate* list_nvc = NonVolatileChunkSegregate::ready_for_use[list_idx];
  NonVolatileChunkSegregate* next_list_nvc = NULL;
  if (list_nvc == NULL) {
    NonVolatileChunkSegregate::ready_for_use[list_idx] = nvc;
    nvc->set_next_chunk(NULL);
    return;
  }
  num_of_empty_slots = nvc->get_num_of_empty_slots();
  if (num_of_empty_slots >= list_nvc->get_num_of_empty_slots()) {
    nvc->set_next_chunk(list_nvc);
    NonVolatileChunkSegregate::ready_for_use[list_idx] = nvc;
    return;
  }
  while (list_nvc->get_next_chunk() != NULL) {
    if (num_of_empty_slots >= list_nvc->get_num_of_empty_slots()) {
      next_list_nvc = list_nvc->get_next_chunk();
      nvc->set_next_chunk(next_list_nvc);
      list_nvc->set_next_chunk(nvc);
      return;
    }
    list_nvc = list_nvc->get_next_chunk();
  }
  // list_nvcのnextがnull．
  list_nvc->set_next_chunk(nvc);
  nvc->set_next_chunk(NULL);
  return;
}



void NonVolatileChunkLarge::mark_object(void* ptr, size_t obj_word_size) {
  NonVolatileChunkLarge* obj_header = (NonVolatileChunkLarge*) (((char*) ptr) - sizeof(NonVolatileChunkLarge));
  obj_header->m_0_to_1();
  return;
}

bool NonVolatileChunkLarge::is_next(NonVolatileChunkLarge* nvcl) {  // TODO: rename this function it is not good enough.
  size_t this_byte_size = sizeof(NonVolatileChunkLarge) + (word_size * HeapWordSize);
  void* next_nvcl_address = (NonVolatileChunkLarge*) (((char*)this) + this_byte_size);
  return nvcl == next_nvcl_address;
}


void NonVolatileChunkLarge::sweep_objects() {
  NonVolatileChunkLarge* nvcl = (NonVolatileChunkLarge*) NVMAllocator::large_head;
  size_t nvcl_byte_size;  // sizeof(Header) + (nvcl->get_word_size() * HeapWordSize)
  // NonVolatileChunkLarge* last_free = (NonVolatileChunkLarge*) NVMAllocator::large_head;
  NonVolatileChunkLarge* last_free = NULL;
  NonVolatileChunkLarge* free_head = (NonVolatileChunkLarge*) NVMAllocator::large_head; // nvclから見て左側の，一番近い空

  while((void*) nvcl < NVMAllocator::nvm_tail) {
    nvcl_byte_size = sizeof(NonVolatileChunkLarge) + (nvcl->get_word_size() * HeapWordSize);
    if (nvcl->get_mark()) {
      // living and marked object.
      // printf("marked object\n");
      nvcl->m_1_to_0();
      nvcl = (NonVolatileChunkLarge*) (((char*)nvcl) + nvcl_byte_size);
      free_head = nvcl;
    } else {
      // not marked. garbage
      // printf("unmarked object\n");
      nvcl->a_1_to_0_anyway();
      if (free_head == nvcl) {
        // 一つ前のオブジェクトは生きている
        if (last_free != NULL) {
          last_free->set_next_chunk(free_head);
        }
        last_free = free_head;
      } else {
        // 一つ前も空
        free_head->set_word_size(free_head->get_word_size() + (nvcl_byte_size / HeapWordSize));
        // free_head->set_word_size(free_head->get_word_size() + nvcl->get_word_size());
      }
          nvcl = (NonVolatileChunkLarge*) (((char*)nvcl) + nvcl_byte_size);
    }

  }
}



// void NonVolatileChunkLarge::sweep_objects() {
//   NonVolatileChunkLarge* nvcl = (NonVolatileChunkLarge*) NVMAllocator::large_head;
//   NonVolatileChunkLarge* last_free_nvcl = (NonVolatileChunkLarge*) NULL;

//   while ((void*)nvcl < NVMAllocator::nvm_tail) {
//     if (nvcl->get_alloc()) {
//       // using nvcl. keep or sweep.
//       if (nvcl->get_mark()) {
//         // keep
//         nvcl->m_1_to_0();
//       } else {
//         // sweep
//         nvcl->a_1_to_0();
//         // printf("large: sweep\n");
//         if (last_free_nvcl != NULL) {
//           if (last_free_nvcl->is_next(nvcl)) {
//             // next to last free chunk. merge.
//             last_free_nvcl->merge(nvcl);
//             nvcl = last_free_nvcl;
//           } else {
//             // not next to each other. just update last_free_chunk.
//             last_free_nvcl->set_next_chunk(nvcl);
//           }
//         }
//         last_free_nvcl = nvcl;
//       }
//     } else {
//       // not used. connect free list
//       if (last_free_nvcl != NULL) last_free_nvcl->set_next_chunk(nvcl);
//       last_free_nvcl = nvcl;
//     }


//       size_t nvcl_byte_size = sizeof(NonVolatileChunkLarge) + ((nvcl->get_word_size()) * HeapWordSize);
//       nvcl = (NonVolatileChunkLarge*) (((char*)nvcl) + nvcl_byte_size);
//   }
// }


void NonVolatileChunkLarge::follow_empty_large_header() {
  printf("follow empty large header start\n");
  NonVolatileChunkLarge*  nvcl = (NonVolatileChunkLarge*) NVMAllocator::large_head;
  while((void*)nvcl < NVMAllocator::nvm_tail) {
    printf("%p %lubytes , next: %p\n", (void*)nvcl, nvcl->get_word_size(), nvcl->get_next_chunk());
    if (nvcl->get_next_chunk() == NULL) {
      printf("follow finished\n");
      return;
    }
    nvcl = nvcl->get_next_chunk();
  }
  printf("follow empty large header finished\n");
  fflush(stdout);
}

void NonVolatileChunkLarge::follow_all_large_header() {
  // printf("follow all large header start\n");
  NonVolatileChunkLarge*  nvcl = (NonVolatileChunkLarge*) NVMAllocator::large_head;
  while((void*)nvcl < NVMAllocator::nvm_tail) {
    // printf("%p %lu(%lu)bytes, allocation: %d, mark: %d, next: %p\n", (void*)nvcl, nvcl->get_word_size()*HeapWordSize, nvcl->get_word_size()*HeapWordSize+sizeof(NonVolatileChunkLarge), nvcl->get_alloc(), nvcl->get_mark(), nvcl->get_next_chunk());
    // if (nvcl->get_next_chunk() == NULL) {
    //   printf("follow all large header finished\n");
    //   return;
    // }
    nvcl = (NonVolatileChunkLarge*) (((char*)nvcl) + sizeof(NonVolatileChunkLarge) + (nvcl->get_word_size() * HeapWordSize));
  }
  // printf("follow all large header inished\n");
  // fflush(stdout);
}





void NonVolatileChunkLarge::m_0_to_1() {
    assert(mark == false, "");
    mark = true;
}

void NonVolatileChunkLarge::m_1_to_0() {
    assert(mark == true, "");
    mark = false;
}
#endif  // NVMGC

#endif // USE_NVTLAB
#endif // OUR_PERSIST