#ifdef OUR_PERSIST
#ifdef USE_NVTLAB

// #include "gc/shared/gcUtil.hpp"
#include "runtime/perfData.hpp"
// #include "runtime/vm_version.hpp"
// #include "nvm/ourPersist.hpp"
// #include "runtime/thread.inline.hpp"

#ifndef NVM_NVTLAB_HPP
#define NVM_NVTLAB_HPP

class NonVolatileChunkSegregate;
// class NonVolatileChunkSmall;
// class NonVolatileChunkMedium;

class ChunkSizeInfo: public CHeapObj<mtGC> {
public:
  size_t min_size;
  size_t max_size;
  ChunkSizeInfo() {}
  ChunkSizeInfo(size_t _min, size_t _max) {
    min_size = _min;
    max_size = _max;
  }
};

class NonVolatileThreadLocalAllocBuffer: public CHeapObj<mtGC> {
// class NonVolatileThreadLocalAllocBuffer: public CHeapObj<mtGC> {
public:
  static const size_t nvm_chunk_byte_size = 4096;
  static const size_t num_of_nvc_small = 31;
  static const size_t num_of_nvc_medium = 8;
  static const size_t num_of_nvc_small_medium = num_of_nvc_small + num_of_nvc_medium;  // 39
  static const size_t segregated_num = num_of_nvc_small_medium + 1; // 40
  static const size_t min_size_small = 1;
  static const size_t max_size_small = 31;
  static const size_t min_size_medium = 32;
  static const size_t max_size_medium = 256;
  static const size_t min_size_large = 257;
  
  static const size_t nvm_size_for_small_medium_G = 15; // size in GB
  static const size_t total_chunks = nvm_size_for_small_medium_G * 1024 * 1024 * 1024 / nvm_chunk_byte_size;

  static ChunkSizeInfo csi[segregated_num];
  static size_t wsize_to_index[min_size_large];
private:
  void* start;
  NonVolatileChunkSegregate* nvc[segregated_num];

public:
  NonVolatileThreadLocalAllocBuffer() {
    for (size_t i = 0; i < segregated_num; i++) {
      nvc[i] = NULL;
    }
  }
  ~NonVolatileThreadLocalAllocBuffer() {}
  inline bool small_or_medium(size_t word_size) {
    size_t idx = word_size_to_idx(word_size);
    return idx < num_of_nvc_small_medium;
  }
  static void initialize_wsize_to_index();
  static void initialize_csi();
  static void startup_initialization();

  Thread* thread();

  // Initialize NVTLAB. It is called from Universe::initialize_nvtlab().
  void initialize();

  // Allocate NVM region via an NVC.
  void* allocate(size_t _word_size);

  // Replace full nvc with a new (or used but non-full) one.
  bool refill(size_t idx, bool must_new);

  // Convert a word size into NVC[] index.
  static size_t word_size_to_idx(size_t word_size);
  static size_t idx_to_word_size(size_t word_size);
  static size_t idx_to_minimum_word_size(size_t word_size);

  static ByteSize start_offset() {
    return byte_offset_of(NonVolatileThreadLocalAllocBuffer, start);
  }
};


#endif // NVM_NVTLAB_HPP
#endif // USE_NVTLAB
#endif // OUR_PERSIST