#ifdef OUR_PERSIST
#ifdef USE_NVTLAB

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
  // bool is_full;
  uint64_t abit[NonVolatileThreadLocalAllocBuffer::kbyte_per_nvc * 1024/(HeapWordSize * sizeof(uint64_t) * 8)];
  uint64_t mbit[NonVolatileThreadLocalAllocBuffer::kbyte_per_nvc * 1024/(HeapWordSize * sizeof(uint64_t) * 8)];
  NonVolatileChunkSegregate* next_chunk;

  static NonVolatileChunkSegregate* standby_for_gc[40];
  static void* nvc_address[3750000]; // FIXME: hardcoding
  static char fill_memory[4];
  static void add_standby_for_gc(NonVolatileChunkSegregate* nvc, size_t idx);
  void set_nvc_address();
  static void* get_nvc_address(size_t idx) { return nvc_address[idx]; }

  virtual bool is_nvc_small() = 0;
  virtual bool is_nvc_medium() = 0;
#ifdef USE_NVTLAB
#ifdef NVMGC
  size_t num_of_empty_slots;
  static NonVolatileChunkSegregate* ready_for_use[40];
  static const float ready_for_use_threshold;
#endif // NVMGC
#endif // USE_NVLTAB

public:
  static pthread_mutex_t gc_mtx;
  NonVolatileChunkSegregate(void* _start, void* _end, size_t _size_class)
  : start(_start)
  , end(_end)
  , size_class(_size_class)
  , allocation_top(0)
  , is_using(true)
  // , is_full(false)
  , next_chunk((NonVolatileChunkSegregate*) NULL)
  {
#ifdef USE_NVTLAB
#ifdef NVMGC
    num_of_empty_slots = 0;
#endif // NVMGC
#endif //USE_NVTLAB
    if (size_class == 0) {
      printf("size_class == 0\n");
      size_class = 1;
    }
    max_idx = 512 / size_class;

    size_t max_idx = NonVolatileThreadLocalAllocBuffer::kbyte_per_nvc*1024/(HeapWordSize * sizeof(u_int64_t)*8);
    for (size_t i = 0; i < max_idx; i++) {
      abit[i] = (uint64_t) 0;
      mbit[i] = (uint64_t) 0;

    }
    set_nvc_address();
  }

  size_t get_allocation_top() { return allocation_top; }
  void set_allication_top(size_t t) { allocation_top = t; }
  
  void print_nvc_info();
  static void add_standby_for_gc_without_lock(NonVolatileChunkSegregate* nvc, size_t idx);
  static NonVolatileChunkSegregate* get_standby_for_gc_head(size_t idx) { return standby_for_gc[idx]; }
  NonVolatileChunkSegregate* get_next_chunk() { return next_chunk; }
  void set_next_chunk(NonVolatileChunkSegregate* nvc) { next_chunk = nvc; }
  size_t get_size_class() { return size_class; }
  size_t get_max_idx()  { return max_idx; }
  bool get_is_using() { return is_using; }
  // bool get_is_full() { return is_full; }
  void set_is_using(bool b)  { is_using = b; }
  // void set_is_full(bool b)  { is_full = b; }
  void* get_start() { return start; }

  bool get_abit(size_t idx);
  void reverse_abit(size_t idx);

  void a_0_to_1(size_t idx);
  void a_1_to_0(size_t idx);

  virtual void* allocation() = 0;
  void* idx_2_address(size_t idx);
  static void* address_to_nvc(void* ptr);
  size_t address_to_idx(void* ptr);

  static NonVolatileChunkSegregate* generate_new_nvc(size_t idx);
  static void initialize_standby_for_gc();
  static void print_standby_for_gc();
#ifdef USE_NVTLAB
#ifdef NVMGC
  static void mark_object(void* ptr, size_t obj_word_size);
  static void sweep_objects();
  static NonVolatileChunkSegregate* get_ready_for_use_head(size_t idx) { return ready_for_use[idx]; }
  static void set_ready_for_use_head(NonVolatileChunkSegregate* nvc, size_t idx) { ready_for_use[idx] = nvc; }
  static void update_ready_for_use();
  static void move_from_standby_to_ready(NonVolatileChunkSegregate* nvc, size_t list_idx);
  static void move_from_ready_to_standby(NonVolatileChunkSegregate* nvc, size_t list_idx);

  bool get_mbit(size_t idx);
  void reverse_mbit(size_t idx);

  void m_0_to_1(size_t idx);
  void m_1_to_0(size_t idx);

  size_t get_num_of_empty_slots() { return num_of_empty_slots; }
  void set_num_of_empty_slots(size_t num)  { num_of_empty_slots = num; }
#endif // NVMGC
#endif // USE_NVTLAB
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

class NonVolatileChunkLarge {
private:
  bool alloc;
  bool mark;
  size_t word_size;
  NonVolatileChunkLarge* next_chunk;
  uint64_t dummy[] __attribute__((aligned(HeapWordSize)));
public:
  NonVolatileChunkLarge(bool _alloc, size_t _word_size, NonVolatileChunkLarge* _next_chunk)
  : alloc(_alloc)
  , mark(false)
  , word_size(_word_size)
  , next_chunk(_next_chunk)
  {
  }

  ~NonVolatileChunkLarge();
  static const size_t MINIMUM_WORD_SIZE_OF_NVCLARGE_ALLOCATION = 257;
  static bool has_available_chunk; // 初回GC前や空きが無いとき(一周探索して空きがないときから、次回GCまで)はfalse
  static void* large_head;

  static void* allocation(size_t word_size);
  void merge(NonVolatileChunkLarge* nvcl);
  static void print_free_list();

  bool get_alloc() { return alloc; }
  size_t get_word_size() { return word_size; }
  void set_word_size(size_t _word_size) { word_size = _word_size; }
  NonVolatileChunkLarge* get_next_chunk() { return next_chunk; }
  void set_next_chunk(NonVolatileChunkLarge* _next_chunk) { next_chunk = _next_chunk; }
  void* get_dummy() { return (void*) dummy; }
  void a_0_to_1();
  void a_1_to_0();
  void a_1_to_0_anyway() { alloc = false; }

#ifdef NVMGC
  bool is_next(NonVolatileChunkLarge* nvcl);
  bool get_mark() { return mark; }
  static void mark_object(void* ptr, size_t obj_word_size);
  static void sweep_objects();
  void m_0_to_1();
  void m_1_to_0();
  static void follow_empty_large_header();
  static void follow_all_large_header();
#endif // NVMGC

};

#endif // NVM_NVTLAB_HPP

#endif // USE_NVTLAB
#endif // OUR_PERSIST