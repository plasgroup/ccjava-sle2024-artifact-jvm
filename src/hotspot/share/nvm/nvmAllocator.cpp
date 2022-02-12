#ifdef OUR_PERSIST

#include "nvm/nvmAllocator.hpp"
#include "runtime/thread.hpp"
#include "utilities/vmError.hpp"
#include "utilities/nativeCallStack.hpp"
#include <sys/mman.h>
#include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"
#include "nvm/nonVolatileChunk.hpp"

void* NVMAllocator::nvm_head = NULL;
void* NVMAllocator::segregated_top = NULL;
void* NVMAllocator::nvm_tail = NULL;
void* NVMAllocator::large_head = NULL;
void* NVMAllocator::large_top = NULL;
NonVolatileChunkLarge* NVMAllocator::first_free_nvcl = (NonVolatileChunkLarge*) NULL;
pthread_mutex_t NVMAllocator::allocate_mtx = PTHREAD_MUTEX_INITIALIZER;

void NVMAllocator::init() {
  struct stat stat_buf;
  const char* nvm_path = XSTR(NVM_FILE_PATH);

  int nvm_fd = open(nvm_path, O_CREAT | O_RDWR, 0666);
  if(nvm_fd < 0) {
    report_vm_error(__FILE__, __LINE__, "Failed to open the file. path: " XSTR(NVM_FILE_PATH));
  }

  fstat(nvm_fd, &stat_buf);
  off_t size = stat_buf.st_size;

#ifdef USE_NVM
  NVMAllocator::nvm_head = (void*)mmap(NULL, size, PROT_WRITE, MAP_SHARED, nvm_fd, 0);
  NVMAllocator::segregated_top = NVMAllocator::nvm_head;
  bool success = NVMAllocator::nvm_head != MAP_FAILED;
  if (!success) {
    report_vm_error(__FILE__, __LINE__, "Failed to map the file.");
  }
#else
  NVMAllocator::nvm_head = (void*)AllocateHeap(size, mtNone, NativeCallStack::empty_stack(), AllocFailStrategy::RETURN_NULL);
  NVMAllocator::segregated_top = NVMAllocator::nvm_head;
  bool success = NVMAllocator::nvm_head != NULL;  //TODO: nvm_topに対応させる
  if (!success) {
    report_vm_error(__FILE__, __LINE__, "Failed to allocate the dram.");
  }
#endif

  NVMAllocator::nvm_tail = (void*)(((char*)NVMAllocator::nvm_head) + size);

#ifdef USE_NVTLAB
  NVMAllocator::large_head = (void*)(((char*)NVMAllocator::nvm_head) + (NVMAllocator::SEGREGATED_REGION_SIZE_GB * 1024 * 1024 * 1024));
  NVMAllocator::large_top = NVMAllocator::large_head;

  // initialize NVCLarge
  uintptr_t uintptr_nvm_tail = (uintptr_t) NVMAllocator::nvm_tail;
  uintptr_t uintptr_large_head = (uintptr_t) NVMAllocator::large_head;
  size_t first_free_nvcl_word_size = (size_t) ((uintptr_nvm_tail - uintptr_large_head - sizeof(NonVolatileChunkLarge)) / HeapWordSize);
  NVMAllocator::first_free_nvcl = new(NVMAllocator::large_head) NonVolatileChunkLarge(false, first_free_nvcl_word_size, NULL);
#endif  // USE_NVTLAB
}

void* NVMAllocator::allocate(size_t _size)
{
  // calc size in bytes
  int size = _size * HeapWordSize;
  void* ptr = NULL;

#ifdef USE_NVTLAB

  if (_size >= NonVolatileChunkLarge::MINIMUM_WORD_SIZE_OF_NVCLARGE_ALLOCATION) {
    ptr = NonVolatileChunkLarge::allocation(_size);
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

  ptr = nvtlab.allocate(_size);

  if (ptr == NULL) {
      printf("allocation failed\n");
    report_vm_error(__FILE__, __LINE__, "NVMAlocator::allocate cannot allocate nvm.");
  }

#elif USE_NVTLAB_BUMP
  Thread* cur_thread = Thread::current();
  void* bump_head = cur_thread->nvtlab_bump_head();
  int bump_size = cur_thread->nvtlab_bump_size();

  // FIXME:
  const int chunk_size = 40 * 1024; // 40kb

  if (size > chunk_size) {
    // large object
    tty->print_cr("allocate size: %d", size);
    // ShouldNotReachHere();
    // report_vm_error(__FILE__, __LINE__, "ShouldNotReachHere");
    pthread_mutex_lock(&NVMAllocator::allocate_mtx);
    if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

    ptr = NVMAllocator::nvm_head;
    void* nvm_next = (void*)(((char*)NVMAllocator::nvm_head) + size);
    NVMAllocator::nvm_head = nvm_next;

    // check
    if (NVMAllocator::nvm_tail <= nvm_next) {
      report_vm_error(__FILE__, __LINE__, "Out of memory in NVM.");
    }
    pthread_mutex_unlock(&NVMAllocator::allocate_mtx);
  } else {
    // small object
    if (size > bump_size || bump_head == NULL) {
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
    cur_thread->set_nvtlab_bump_head((void*)(((char*)bump_head) + size));
    cur_thread->set_nvtlab_bump_size(bump_size - size);
  }
#else
  // 大域的な領域からメモリを確保する
  // allocate
  pthread_mutex_lock(&NVMAllocator::allocate_mtx);
  if (NVMAllocator::nvm_head == NULL) NVMAllocator::init();

  ptr = NVMAllocator::nvm_head;
  void* nvm_next = (void*)(((char*)NVMAllocator::nvm_head) + size);
  NVMAllocator::nvm_head = nvm_next;

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
