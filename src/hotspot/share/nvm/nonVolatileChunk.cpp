#ifdef OUR_PERSIST
#ifdef USE_NVTLAB

#include "precompiled.hpp"
#include "nvm/ourPersist.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "runtime/thread.inline.hpp"
#include "nvm/nvmAllocator.hpp"
#include <pthread.h>

pthread_mutex_t NonVolatileChunkSegregate::gc_mtx = PTHREAD_MUTEX_INITIALIZER;
NonVolatileChunkSegregate* NonVolatileChunkSegregate::standby_for_gc[NonVolatileThreadLocalAllocBuffer::segregated_num];
size_t NonVolatileChunkSegregate::count_1_on_uint64_t[256];

void* NonVolatileChunkSegregate::nvc_address[NonVolatileThreadLocalAllocBuffer::total_chunks] = {NULL};
bool NonVolatileChunkLarge::has_available_chunk = false;

#ifdef USE_NVTLAB
#ifdef NVMGC
NonVolatileChunkSegregate* NonVolatileChunkSegregate::ready_for_use[NonVolatileThreadLocalAllocBuffer::segregated_num];
const float NonVolatileChunkSegregate::ready_for_use_threshold = 0.1;
#endif // NVMGC
#endif // USE_NVTLAB

void NonVolatileChunkSegregate::initialize_standby_for_gc() {
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i++) {
    standby_for_gc[i] = NULL;
  }
}

void NonVolatileChunkSegregate::set_nvc_address() {
  if (start == NULL) return;
  uintptr_t segregated_head = (uintptr_t) NVMAllocator::nvm_head;
  uintptr_t nvc_head = (uintptr_t) start;
  uintptr_t dif = (nvc_head - segregated_head) / NonVolatileThreadLocalAllocBuffer::nvm_chunk_byte_size;
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

NonVolatileChunkSegregate* NonVolatileChunkSegregate::generate_new_nvc(size_t idx) {
  pthread_mutex_lock(&NVMAllocator::allocate_mtx);
  void *_start = NVMAllocator::segregated_top;
  void *_end = NVMAllocator::allocate_chunksize();
  pthread_mutex_unlock(&NVMAllocator::allocate_mtx);

  size_t _min_size = NonVolatileThreadLocalAllocBuffer::idx_to_minimum_word_size(idx);
  size_t _size_class = NonVolatileThreadLocalAllocBuffer::idx_to_word_size(idx);
  NonVolatileChunkSegregate* nvc = new NonVolatileChunkSegregate(_start, _end, _min_size, _size_class);
  add_standby_for_gc(nvc, idx);
  return nvc;
}

void NonVolatileChunkSegregate::add_standby_for_gc(NonVolatileChunkSegregate* nvc, size_t idx) {
  pthread_mutex_lock(&gc_mtx);
  assert(idx < NonVolatileThreadLocalAllocBuffer::segregated_num, "");
  NonVolatileChunkSegregate* nvc_list_head = standby_for_gc[idx];
  nvc->next_chunk = nvc_list_head;
  standby_for_gc[idx] = nvc;
  pthread_mutex_unlock(&gc_mtx);
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
  for (size_t i = 0; i < nslots; i++) {
    abit1 += abit[i];
    mbit1 += mbit[i];
  }
  printf("abit 1: %lu\n", abit1);
  printf("     0: %lu\n", nslots);
  printf("mbit 1: %lu\n", mbit1);
  printf("     0: %lu\n", nslots);
  printf("----------\n");
}

bool NonVolatileChunkSegregate::get_abit(size_t idx) {
  assert(idx <= nslots, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool ret = (abit[base] & mask) != 0;
  return ret;
}

void NonVolatileChunkSegregate::a_0_to_1(size_t idx) {
  assert(idx <= nslots, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (abit[base] & mask) != 0;
  assert(b == false, "");
  abit[base] = abit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::a_1_to_0(size_t idx) {
  assert(idx <= nslots, "");
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
  uintptr_t dif = (ptr_uint - segregated_head) / NonVolatileThreadLocalAllocBuffer::nvm_chunk_byte_size;
  return NonVolatileChunkSegregate::get_nvc_address(dif);
}

size_t NonVolatileChunkSegregate::address_to_idx(void* ptr) {
  uintptr_t ptr_uint = (uintptr_t) ptr;
  uintptr_t start_uint = (uintptr_t) start;
  size_t dif = (ptr_uint - start_uint) / (HeapWordSize * size_class);
  return dif;
}

void* NonVolatileChunkSegregate::allocation() {
  for (size_t i = allocation_top; i < nslots; i++) {
    if (get_abit(i) == false) {
      char* ad = (char*) idx_2_address(i);
      a_0_to_1(i);
      allocation_top = i + 1;
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
      if (nvcl_size > word_size + ((sizeof(NonVolatileChunkLarge)-1) / HeapWordSize) + 1 + NonVolatileThreadLocalAllocBuffer::min_size_large) {
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
  assert(idx <= nslots, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool ret = (mbit[base] & mask) != 0;
  return ret;
}

void NonVolatileChunkSegregate::m_0_to_1(size_t idx) {
  assert(idx <= nslots, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (mbit[base] & mask) != 0;
  assert(b == false, "");
  mbit[base] = mbit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::m_1_to_0(size_t idx) {
  assert(idx <= nslots, "");
  size_t base = idx / 64;
  size_t offset = idx % 64;
  uint64_t mask = 1ULL << offset;
  bool b = (mbit[base] & mask) != 0;
  assert(b == true, "");
  mbit[base] = mbit[base] ^ mask;
  return;
}

void NonVolatileChunkSegregate::clear_mbit() {
  size_t n = (nslots + sizeof(uint64_t) - 1) / sizeof(uint64_t); // required number of uint64_t for nslots.
  for (size_t i = 0; i < n; i++) {
    mbit[i] = (uint64_t) 0;
  }
}

void NonVolatileChunkSegregate::initialize_count_1_on_uint64_t() {
NonVolatileChunkSegregate::count_1_on_uint64_t[0] = 0;
NonVolatileChunkSegregate::count_1_on_uint64_t[1] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[2] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[3] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[4] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[5] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[6] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[7] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[8] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[9] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[10] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[11] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[12] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[13] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[14] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[15] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[16] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[17] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[18] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[19] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[20] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[21] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[22] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[23] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[24] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[25] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[26] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[27] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[28] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[29] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[30] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[31] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[32] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[33] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[34] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[35] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[36] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[37] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[38] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[39] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[40] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[41] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[42] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[43] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[44] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[45] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[46] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[47] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[48] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[49] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[50] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[51] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[52] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[53] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[54] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[55] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[56] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[57] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[58] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[59] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[60] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[61] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[62] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[63] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[64] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[65] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[66] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[67] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[68] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[69] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[70] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[71] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[72] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[73] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[74] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[75] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[76] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[77] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[78] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[79] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[80] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[81] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[82] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[83] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[84] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[85] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[86] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[87] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[88] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[89] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[90] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[91] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[92] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[93] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[94] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[95] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[96] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[97] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[98] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[99] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[100] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[101] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[102] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[103] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[104] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[105] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[106] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[107] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[108] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[109] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[110] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[111] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[112] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[113] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[114] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[115] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[116] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[117] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[118] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[119] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[120] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[121] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[122] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[123] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[124] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[125] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[126] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[127] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[128] = 1;
NonVolatileChunkSegregate::count_1_on_uint64_t[129] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[130] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[131] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[132] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[133] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[134] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[135] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[136] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[137] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[138] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[139] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[140] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[141] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[142] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[143] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[144] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[145] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[146] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[147] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[148] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[149] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[150] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[151] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[152] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[153] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[154] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[155] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[156] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[157] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[158] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[159] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[160] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[161] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[162] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[163] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[164] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[165] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[166] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[167] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[168] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[169] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[170] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[171] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[172] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[173] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[174] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[175] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[176] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[177] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[178] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[179] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[180] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[181] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[182] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[183] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[184] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[185] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[186] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[187] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[188] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[189] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[190] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[191] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[192] = 2;
NonVolatileChunkSegregate::count_1_on_uint64_t[193] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[194] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[195] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[196] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[197] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[198] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[199] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[200] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[201] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[202] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[203] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[204] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[205] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[206] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[207] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[208] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[209] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[210] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[211] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[212] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[213] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[214] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[215] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[216] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[217] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[218] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[219] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[220] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[221] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[222] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[223] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[224] = 3;
NonVolatileChunkSegregate::count_1_on_uint64_t[225] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[226] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[227] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[228] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[229] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[230] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[231] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[232] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[233] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[234] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[235] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[236] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[237] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[238] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[239] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[240] = 4;
NonVolatileChunkSegregate::count_1_on_uint64_t[241] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[242] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[243] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[244] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[245] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[246] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[247] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[248] = 5;
NonVolatileChunkSegregate::count_1_on_uint64_t[249] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[250] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[251] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[252] = 6;
NonVolatileChunkSegregate::count_1_on_uint64_t[253] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[254] = 7;
NonVolatileChunkSegregate::count_1_on_uint64_t[255] = 8;
}

size_t NonVolatileChunkSegregate::count_abit_1() {
  size_t count = 0;
  size_t n = (nslots + sizeof(uint64_t) - 1) / sizeof(uint64_t); // required number of unsigned char for nslots.
  unsigned char * ucabit = (unsigned char *) abit;
  for (size_t i = 0; i < n; i++) {
    unsigned char num = ucabit[i];
    count += NonVolatileChunkSegregate::count_1_on_uint64_t[num];
  }
  return count;
}

inline void NonVolatileChunkSegregate::swap_abit_mbit() {
  uint64_t* tmp = abit;
  abit = mbit;
  mbit = tmp;
}

void NonVolatileChunkSegregate::mark_object(void* ptr, size_t obj_word_size) {
  if (obj_word_size >= NonVolatileThreadLocalAllocBuffer::min_size_large) {
        report_vm_error(__FILE__, __LINE__, "NonVolatileChunkSegregate::mark_object cannot mark large objects.");
  }
  NonVolatileChunkSegregate* nvc = (NonVolatileChunkSegregate *) NonVolatileChunkSegregate::address_to_nvc(ptr);
  size_t idx = nvc->address_to_idx(ptr);
  nvc->m_0_to_1(idx);
  return;
}

void NonVolatileChunkSegregate::sweep_objects() {

  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    NonVolatileChunkSegregate* nvc = standby_for_gc[i];
    while (nvc != NULL) {
      nvc->swap_abit_mbit();
      nvc->clear_mbit();
      size_t free_count = nvc->get_nslots() - nvc->count_abit_1();
      nvc->set_num_of_empty_slots(free_count);
      nvc->set_allication_top(0);
      nvc = nvc->next_chunk;
    }
  }
  for (size_t i = 0; i < NonVolatileThreadLocalAllocBuffer::segregated_num; i ++) {
    NonVolatileChunkSegregate* nvc = ready_for_use[i];
    while (nvc != NULL) {
      nvc->swap_abit_mbit();
      nvc->clear_mbit();
      size_t free_count = nvc->get_nslots() - nvc->count_abit_1();
      nvc->set_num_of_empty_slots(free_count);
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
        float next_nvc_free_slots_percent = (float)next_chunk->get_num_of_empty_slots() / (float)next_chunk->get_nslots() * 100;
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