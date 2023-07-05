#ifdef OUR_PERSIST

#include "nvm/memory/nvmMetaData.hpp"
#include "nvm/nonVolatileChunk.hpp"
#include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"
#include "nvm/nvmAllocator.hpp"
#include "nvm/ourPersist.inline.hpp"
#include "runtime/thread.hpp"
#include "utilities/nativeCallStack.hpp"
#include "utilities/vmError.hpp"
#include <sys/mman.h>

void* NVMAllocator::nvm_head = NULL;
void* NVMAllocator::segregated_top = NULL;
void* NVMAllocator::nvm_tail = NULL;
void* NVMAllocator::large_head = NULL;
void* NVMAllocator::large_top = NULL;
NonVolatileChunkLarge* NVMAllocator::first_free_nvcl = (NonVolatileChunkLarge*) NULL;
pthread_mutex_t NVMAllocator::allocate_mtx = PTHREAD_MUTEX_INITIALIZER;

void NVMAllocator::init(const char* nvm_path, const char* nvm_path_for_size) {
  if (nvm_head != NULL) {
    assert(false, "NVMAllocator::init() is called twice");
  }

  // get the size of the NVM file.
  off_t size = 0;
  {
    const char* nvm_path_for_size = XSTR(NVM_FILE_PATH);
    struct stat stat_buf;
    int nvm_fd_for_size = open(nvm_path_for_size, O_RDWR, 0666);
    if(nvm_fd_for_size < 0) {
      report_vm_error(__FILE__, __LINE__,
                      "Failed to open the file.", "nvm_path_for_size: %s", nvm_path_for_size);
    }
    fstat(nvm_fd_for_size, &stat_buf);
    size = stat_buf.st_size;
  }

  // map the NVM file.
  void* nvm_addr = NULL;
  void* map_addr = NVMAllocator::map_addr();
  {
    int nvm_fd = open(nvm_path, O_RDWR, 0666);
    if(nvm_fd < 0) {
      report_vm_error(__FILE__, __LINE__,
                      "Failed to open the file.", "nvm_path: %s", nvm_path);
    }
    nvm_addr = (void*)mmap(map_addr, size, PROT_WRITE, MAP_SHARED, nvm_fd, 0);
    if (nvm_addr == MAP_FAILED) {
      report_vm_error(__FILE__, __LINE__,
                      "Failed to map the file.",
                      "nvm_path: %s, nvm_addr: %p, map_addr: %p, error massage: %s",
                      nvm_path, nvm_addr, map_addr, strerror(errno));
    }
    if (nvm_addr != map_addr) {
      report_vm_error(__FILE__, __LINE__,
                      "Failed to map the file.",
                      "nvm_path: %s, nvm_addr: %p, map_addr: %p",
                      nvm_path, nvm_addr, map_addr);
    }
  }

  // DEBUG:
  tty->print_cr("map addr: %p", nvm_addr);
  tty->print_cr("NVMMetaData->_state_flag: %ld", NVMMetaData::meta()->_state_flag);
  tty->print_cr("NVMMetaData->_nvm_head: %p", NVMMetaData::meta()->_nvm_head);
  tty->print_cr("NVMMetaData->_mirrors_head: %p", NVMMetaData::meta()->_mirrors_head);

  // initialize the NVM allocator.
  NVMAllocator::nvm_head = (char*)nvm_addr + sizeof(NVMMetaData);
  NVMAllocator::segregated_top = NVMAllocator::nvm_head;
  NVMAllocator::nvm_tail = (void*)(((char*)NVMAllocator::nvm_head) + size);

  // initialize the NVM headers.
#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
  NVMMetaData::meta()->_state_flag = 0;
  NVMMetaData::meta()->_mirrors_head = NULL;
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE
  if (NVMMetaData::meta()->_state_flag == 1) {
    NVMAllocator::nvm_head = NVMMetaData::meta()->_nvm_head;
  } else {
    NVMMetaData::meta()->_nvm_head = NVMAllocator::nvm_head;
    NVM_WRITEBACK(NVMMetaData::meta()->nvm_head_addr());
  }
  tty->print_cr("NVMAllocator::nvm_head: %p", NVMAllocator::nvm_head);

#ifdef USE_NVTLAB
  NonVolatileThreadLocalAllocBuffer::initialize_csi();
  NonVolatileThreadLocalAllocBuffer::initialize_wsize_to_index();
#ifdef NVMGC
  NonVolatileChunkSegregate::initialize_count_1_on_uint64_t();
#endif // NVMGC
  NVMAllocator::large_head = (void*)(((char*)NVMAllocator::nvm_head) + (NVMAllocator::SEGREGATED_REGION_SIZE_GB * 1024 * 1024 * 1024));
  NVMAllocator::large_top = NVMAllocator::large_head;

  // initialize NVCLarge
  uintptr_t uintptr_nvm_tail = (uintptr_t) NVMAllocator::nvm_tail;
  uintptr_t uintptr_large_head = (uintptr_t) NVMAllocator::large_head;
  size_t first_free_nvcl_word_size = (size_t) ((uintptr_nvm_tail - uintptr_large_head - sizeof(NonVolatileChunkLarge)) / HeapWordSize);
  NVMAllocator::first_free_nvcl = new(NVMAllocator::large_head) NonVolatileChunkLarge(false, first_free_nvcl_word_size, NULL);
#endif  // USE_NVTLAB
}

void* NVMAllocator::allocate(size_t _word_size) {
  // calc size in bytes
  int _byte_size = _word_size * HeapWordSize;
  void* ptr = NULL;

#ifdef USE_NVTLAB

  if (_word_size >= NonVolatileChunkLarge::MINIMUM_WORD_SIZE_OF_NVCLARGE_ALLOCATION) {
    ptr = NonVolatileChunkLarge::allocation(_word_size);
    return ptr;
  }

  // NVTLABからメモリを確保する

  if (NVMAllocator::nvm_head == NULL){
    pthread_mutex_lock(&NVMAllocator::allocate_mtx);
    NVMAllocator::init();
    NonVolatileChunkSegregate::initialize_standby_for_gc();
    pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
  }
  Thread* thr = Thread::current();
  NonVolatileThreadLocalAllocBuffer& nvtlab = thr->nvtlab();

  ptr = nvtlab.allocate(_word_size);

  if (ptr == NULL) {
      printf("allocation failed\n");
    report_vm_error(__FILE__, __LINE__, "NVMAlocator::allocate cannot allocate nvm.");
  }

#elif defined USE_NVTLAB_BUMP
  Thread* cur_thread = Thread::current();
  void* bump_head = cur_thread->nvtlab_bump_head();
  int bump_size = cur_thread->nvtlab_bump_size();

#ifndef NVTLAB_BUMP_CHUNK_SIZE_KB
  report_vm_error(__FILE__, __LINE__, "NVTLAB_BUMP_CHUNK_SIZE_KB is not defined.");
#endif // NVTLAB_BUMP_CHUNK_SIZE_KB
  const int chunk_size = NVTLAB_BUMP_CHUNK_SIZE_KB * 1024;

#ifdef AVOID_SAME_CACHELINE_ALLOCATION
  _byte_size = (_byte_size + 63) & ~63;
#endif // AVOID_SAME_CACHELINE_ALLOCATION

  if (_byte_size > 256 * HeapWordSize) {
    // large object
#ifdef ASSERT
    tty->print_cr("allocate size: %d", _byte_size);
#endif // ASSERT
    pthread_mutex_lock(&NVMAllocator::allocate_mtx);
    if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

    ptr = NVMAllocator::nvm_head;
    void* nvm_next = (void*)(((char*)NVMAllocator::nvm_head) + _byte_size);
    NVMAllocator::nvm_head = nvm_next;

    // check
    if (NVMAllocator::nvm_tail <= nvm_next) {
      report_vm_error(__FILE__, __LINE__, "Out of memory in NVM.");
    }
    pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
  } else {
    // small object
    if (_byte_size > bump_size || bump_head == NULL) {
      // allocate chunk
      pthread_mutex_lock(&NVMAllocator::allocate_mtx);
      if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

      bump_head = NVMAllocator::nvm_head;
      bump_size = chunk_size;
      void* nvm_next = (void*)(((char*)NVMAllocator::nvm_head) + chunk_size);
      NVMAllocator::nvm_head = nvm_next;

      // check
      if (NVMAllocator::nvm_tail <= nvm_next) {
        report_vm_error(__FILE__, __LINE__, "Out of memory in NVM.");
      }
      pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
    }
    // allocate object
    ptr = bump_head;
    cur_thread->set_nvtlab_bump_head((void*)(((char*)bump_head) + _byte_size));
    cur_thread->set_nvtlab_bump_size(bump_size - _byte_size);
  }
#else
  // 大域的な領域からメモリを確保する
  // allocate
  pthread_mutex_lock(&NVMAllocator::allocate_mtx);
  if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

  ptr = NVMAllocator::nvm_head;
  void* nvm_next = (void*)(((char*)NVMAllocator::nvm_head) + _byte_size);
  NVMAllocator::nvm_head = nvm_next;

  NVMMetaData::meta()->_nvm_head = NVMAllocator::nvm_head;
  NVM_WRITEBACK(NVMMetaData::meta()->nvm_head_addr());

  // check
  if (NVMAllocator::nvm_tail <= nvm_next) {
    report_vm_error(__FILE__, __LINE__, "Out of memory in NVM.");
  }
  pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
#endif

  return ptr;
}

void* NVMAllocator::allocate_chunksize() {
  if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

  void* ptr = NVMAllocator::segregated_top;
  void* nvm_next = (void*)(((char*)NVMAllocator::segregated_top) + NVMAllocator::NVM_CHUNK_BYTE_SIZE);
  NVMAllocator::segregated_top = nvm_next;
    // check
  if (NVMAllocator::large_head <= NVMAllocator::segregated_top) {
    report_vm_error(__FILE__, __LINE__, "Out of memory in NVM.");
  }

  return nvm_next;  // not ptr
}


#endif // OUR_PERSIST
