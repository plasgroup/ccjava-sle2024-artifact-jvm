// #ifdef OUR_PERSIST
// #include "gc/shared/gcUtil.hpp"
// #include "runtime/perfData.hpp"
#include "runtime/vm_version.hpp"
// #include "nvm/ourPersist.hpp"
// #include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"

#ifndef NVM_NVC_HPP
#define NVM_NVC_HPP

class NonVolatileThreadLocalAllocBuffer;

class NonVolatileChunkSegregate: public CHeapObj<mtGC> {
protected:
  void* start;
  void* end;
  size_t size_class;
  size_t max_idx;
  bool is_using;
  bool is_full;
  // uint64_t abit[NonVolatileThreadLocalAllocBuffer::kbyte_ber_nvc * 1024/(HeapWordSize * sizeof(unit64_t) * 8)]; // TODO:1 bit演算にする
  // uint64_t mbit[NonVolatileThreadLocalAllocBuffer::kbyte_ber_nvc * 1024/(HeapWordSize * sizeof(unit64_t) * 8)];
  bool abit[512]; // １つのNVCの最大オブジェクト数
  bool mbit[512]; // １つのNVCの最大オブジェクト数
  NonVolatileChunkSegregate* next_chunk;

  static NonVolatileChunkSegregate* standby_for_gc[40];
  static void add_standby_for_gc(NonVolatileChunkSegregate* nvc, size_t idx);

public:
  static pthread_mutex_t gc_mtx;
  NonVolatileChunkSegregate(void* _start, void* _end, size_t _size_class)
  : start(_start)
  , end(_end)
  , size_class(_size_class)
  , is_using(true)
  , is_full(false)
  , next_chunk(NULL)
  {
    if (size_class == 0) {
      printf("size_class == 0\n");
      size_class = 1;
    }
    max_idx = 512 / size_class;

    for (size_t i = 0; i < max_idx; i++) {
      abit[i] = false;
      mbit[i] = false;
    }

    printf("segregated constractor\n");
    fflush(stdout);

  }

  size_t get_max_idx()  { return max_idx; }
  bool get_abit(size_t idx);
  bool get_mbit(size_t idx);
  void reverse_abit(size_t idx);
  void reverse_mbit(size_t idx);

  virtual void* allocation() = 0;
  void* idx_2_address(size_t idx);

  static NonVolatileChunkSegregate* generate_new_nvc(size_t idx);
  static void initialize_standby_for_gc();
  static void print_standby_for_gc();
};

class NonVolatileChunkSmall: public NonVolatileChunkSegregate {
public:
  NonVolatileChunkSmall(void* _start, void* _end, size_t _size_class)
  : NonVolatileChunkSegregate(_start, _end, _size_class)
  {
    printf("small constractor\n");
    fflush(stdout);
  }

  void* allocation();
};

class NonVolatileChunkMedium: public NonVolatileChunkSegregate {
private:
  size_t size_class_minimum;
public:
  NonVolatileChunkMedium(void* _start, void* _end, size_t _size_class, size_t _size_class_minimum)
  : NonVolatileChunkSegregate(_start, _end, _size_class)
  , size_class_minimum(_size_class_minimum)
  {
    if (size_class == 0)
    {
      printf("size_class == 0\n");
      size_class = 1;
    }
  }

  void* allocation();
};

#endif // NVM_NVTLAB_HPP

// #endif // OUR_PERSIST