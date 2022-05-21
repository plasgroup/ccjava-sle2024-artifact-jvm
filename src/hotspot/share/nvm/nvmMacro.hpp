#ifdef OUR_PERSIST

#include <x86intrin.h>
#include <clwbintrin.h>

#define OOP_TO_VOID(obj) static_cast<void*>(obj)

// writeback
#ifdef ENABLE_NVM_WRITEBACK

// flush
#ifdef USE_CLWB
#define NVM_FLUSH(mem) {\
  _mm_clwb(mem);\
  NVM_COUNTER_ONLY(Thread::current()->nvm_counter()->inc_clwb());\
}
#else  // USE_CLWB
#define NVM_FLUSH(mem) {\
  _mm_clflush(mem);\
}
#endif // USE_CLWB

// flush
#ifdef USE_CLWB
#define NVM_FLUSH_FOR_LOOP(mem) {\
  _mm_clwb(mem);\
}
#else  // USE_CLWB
#define NVM_FLUSH_FOR_LOOP(mem) {\
  _mm_clflush(mem);\
}
#endif // USE_CLWB

// flush some cacheline
#define NVM_FLUSH_LOOP(mem, len) {\
  for (ptrdiff_t i = 0; i < (ptrdiff_t)(len); i += 64) {\
    NVM_FLUSH_FOR_LOOP(((u1*)mem) + i)\
  }\
}

// fence
#define NVM_FENCE {\
  _mm_sfence();\
}

// flush and fence
#define NVM_WRITEBACK(mem) {\
  NVM_FLUSH(mem);\
  NVM_FENCE\
}

#define NVM_WRITEBACK_LOOP(mem, len) {\
  NVM_FLUSH_LOOP(mem, len)\
  NVM_FENCE\
}

#else  // ENABLE_NVM_WRITEBACK

#define NVM_FLUSH(mem)
#define NVM_FLUSH_LOOP(mem, len)
#define NVM_FENCE
#define NVM_WRITEBACK(mem)
#define NVM_WRITEBACK_LOOP(mem, len)

#endif // ENABLE_NVM_WRITEBACK


#endif // OUR_PERSIST
