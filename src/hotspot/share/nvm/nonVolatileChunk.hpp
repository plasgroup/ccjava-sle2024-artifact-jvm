// #ifdef OUR_PERSIST
// #include "gc/shared/gcUtil.hpp"
// #include "runtime/perfData.hpp"
#include "runtime/vm_version.hpp"
// #include "nvm/ourPersist.hpp"
// #include "nvm/nonVolatileThreadLocalAllocBuffer.hpp"

#ifndef NVM_NVC_HPP
#define NVM_NVC_HPP

class NonVolatileThreadLocalAllocBuffer;
class NVMAllocator;

class NonVolatileChunkSegregate: public CHeapObj<mtGC> {
protected:
  void* start;
  void* end;
  size_t size_class;
  size_t max_idx;
  size_t allocation_top;
  bool is_using;
  bool is_full;
  // uint64_t abit[NonVolatileThreadLocalAllocBuffer::kbyte_ber_nvc * 1024/(HeapWordSize * sizeof(unit64_t) * 8)]; // TODO:1 bit演算にする
  // uint64_t mbit[NonVolatileThreadLocalAllocBuffer::kbyte_ber_nvc * 1024/(HeapWordSize * sizeof(unit64_t) * 8)];
  bool abit[512]; // １つのNVCの最大オブジェクト数
  bool mbit[512]; // １つのNVCの最大オブジェクト数
  NonVolatileChunkSegregate* next_chunk;

  static NonVolatileChunkSegregate* standby_for_gc[40];
  static void* nvc_address[3750000]; // FIXME: hardcoding
  static char fill_memory[4];
  static void add_standby_for_gc(NonVolatileChunkSegregate* nvc, size_t idx);
  void set_nvc_address();
  static void* get_nvc_address(size_t idx) { return nvc_address[idx]; }

  virtual bool is_nvc_small() = 0;
  virtual bool is_nvc_medium() = 0;

public:
  static pthread_mutex_t gc_mtx;
  NonVolatileChunkSegregate(void* _start, void* _end, size_t _size_class)
  : start(_start)
  , end(_end)
  , size_class(_size_class)
  , allocation_top(0)
  , is_using(true)
  , is_full(false)
  , next_chunk((NonVolatileChunkSegregate*) NULL)
  {
    printf("start: %p, end: %p\n", start, end);
    if (size_class == 0) {
      printf("size_class == 0\n");
      size_class = 1;
    }
    max_idx = 512 / size_class;

    for (size_t i = 0; i < max_idx; i++) {
      abit[i] = false;
      mbit[i] = false;
    }

    set_nvc_address();

  }

  size_t get_allocation_top() { return allocation_top; }
  void set_allication_top(size_t t) { allocation_top = t; }
  
  void print_nvc_info();
  static NonVolatileChunkSegregate* get_standby_for_gc_head(size_t idx) { return standby_for_gc[idx]; }
  NonVolatileChunkSegregate* get_next_chunk() { return next_chunk; }
  size_t get_size_class() { return size_class; }
  size_t get_max_idx()  { return max_idx; }
  bool get_is_using() { return is_using; }
  bool get_is_full() { return is_full; }
  void set_is_using(bool b)  { is_using = b; }
  void set_is_full(bool b)  { is_full = b; }
  void* get_start() { return start; }

  bool get_abit(size_t idx);
  bool get_mbit(size_t idx);
  void reverse_abit(size_t idx);
  void reverse_mbit(size_t idx);
  void a_0_to_1(size_t idx);
  void a_1_to_0(size_t idx);
  void m_0_to_1(size_t idx);
  void m_1_to_0(size_t idx);

  virtual void* allocation() = 0;
  void* idx_2_address(size_t idx);
  static void* address_to_nvc(void* ptr);
  size_t address_to_idx(void* ptr);

  static NonVolatileChunkSegregate* generate_new_nvc(size_t idx);
  static void initialize_standby_for_gc();
  static void print_standby_for_gc();


  static void mark_object(void* ptr, size_t obj_word_size);
  static void sweep_objects();
};

class NonVolatileChunkSmall: public NonVolatileChunkSegregate {
public:
  NonVolatileChunkSmall(void* _start, void* _end, size_t _size_class)
  : NonVolatileChunkSegregate(_start, _end, _size_class)
  {
  }

  void* allocation();
  bool is_nvc_small() { return true; }
  bool is_nvc_medium() { return false; }
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
  bool is_nvc_small() { return false; }
  bool is_nvc_medium() { return true; }
};

#endif // NVM_NVTLAB_HPP

// #endif // OUR_PERSIST