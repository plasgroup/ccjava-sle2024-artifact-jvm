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
class NonVolatileChunkSmall;
class NonVolatileChunkMedium;

class NonVolatileThreadLocalAllocBuffer: public CHeapObj<mtGC> {
public:
  static const size_t kbyte_per_nvc = 4;
  static const size_t segregated_num = 40; // segregated_num should be small + medium.
  static const size_t num_of_nvc_small = 31;
  static const size_t num_of_nvc_medium = 9;
  // static const size_t num_of_nvc_large = 1;
  // static const size_t minimum_word_size_of_extra_large = 257;
private:
  void* start;
  NonVolatileChunkSegregate* nvc[segregated_num];

public:
  NonVolatileThreadLocalAllocBuffer() {
    for (size_t i=0; i < segregated_num; i++) {
      nvc[i] = NULL;
    }
  }
  ~NonVolatileThreadLocalAllocBuffer() {}

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

  // static NonVolatileChunkSegregate* generate_new_nvc(size_t idx);
};

#endif // NVM_NVTLAB_HPP

#endif // USE_NVTLAB
#endif // OUR_PERSIST