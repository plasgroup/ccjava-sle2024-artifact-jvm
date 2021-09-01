#ifdef OUR_PERSIST

#include "nvm/nvmAllocator.hpp"
#include "runtime/thread.hpp"
#include "utilities/vmError.hpp"
#include "utilities/nativeCallStack.hpp"
#include <sys/mman.h>

void* NVMAllocator::nvm_head = NULL;
void* NVMAllocator::nvm_tail = NULL;
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
  bool success = NVMAllocator::nvm_head != MAP_FAILED;
  if (!success) {
    report_vm_error(__FILE__, __LINE__, "Failed to map the file.");
  }
#else
  NVMAllocator::nvm_head = (void*)AllocateHeap(size, mtNone, NativeCallStack::empty_stack(), AllocFailStrategy::RETURN_NULL);
  bool success = NVMAllocator::nvm_head != NULL;
  if (!success) {
    report_vm_error(__FILE__, __LINE__, "Failed to allocate the dram.");
  }
#endif

  NVMAllocator::nvm_tail = (void*)(((char*)NVMAllocator::nvm_head) + size);
}

void* NVMAllocator::allocate(size_t _size) {
  // calc size in bytes
  int size = _size * HeapWordSize;
  void* ptr = NULL;

#ifdef USE_NVTLAB
  // NVTLABからメモリを確保する
  if (NVM::nvm_head == NULL) NVM::init_alloc_nvm();
  Thread* thr = Thread::current();
  NonVolatileThreadLocalAllocBuffer& nvtlab = thr->nvtlab();

  if (nvtlab.get_start() == NULL) {
    nvtlab.initialize();
  }

  if (_size*HeapWordSize >= NonVolatileThreadLocalAllocBuffer::kbyte_per_nvtlab*1024 / 2) {
    ptr = nvtlab.alloc_nvm_outside_nvtlab(_size);
  } else {
    ptr = nvtlab.alloc_nvm_from_nvtlab(_size);
  }

  if (ptr == NULL) {
    // refill
    bool succeeded = nvtlab.refill();
    if (!succeeded) {
      VMError::report_and_die("refill failed");
    }
    // once failed but try again after refill
    ptr = nvtlab.alloc_nvm_from_nvtlab(_size);
    if (ptr == NULL) {
      printf("ptr: %ld\n", _size * HeapWordSize);
      VMError::report_and_die("refill failed 2");
    }
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

#endif // OUR_PERSIST
