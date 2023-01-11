#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSetRuntime.hpp"

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"

void* NVMCardTableBarrierSetRuntime::write_oop_nvm_field_post_entry(
    DecoratorSet decorators, BasicType type) {
  decorators |= OURPERSIST_BS_ASM;
  // auto store_func = (void(*)(oop, ptrdiff_t,
  // oop))CallRuntimeBarrierSet::store_in_heap_at_ptr(decorators, type);
  return (void*)(void (*)(oop, ptrdiff_t, oop))
      CallRuntimeBarrierSet::store_in_heap_at_ptr(decorators, type);
  // store_func(obj, offset, value);
  // switch (type) {
  //   case T_BYTE:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jbyte))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jbyte>;
  //     break;
  //   case T_CHAR:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jchar))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jchar>;
  //     break;
  //   case T_DOUBLE:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jdouble))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jdouble>;
  //     break;
  //   case T_FLOAT:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jfloat))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jfloat>;
  //     break;
  //   case T_INT:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jint))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jint>;
  //     break;
  //   case T_LONG:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jlong))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jlong>;
  //     break;
  //   case T_SHORT:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jshort))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jshort>;
  //     break;
  //   case T_BOOLEAN:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jboolean))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jboolean>;
  //     break;
  //   case T_ADDRESS:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, jlong))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //                                                              jlong>;
  //     break;
  //   case T_OBJECT:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, oop))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //         oop>;
  //     break;
  //   case T_ARRAY:
  //     return (void *)(void (*)(oopDesc *, ptrdiff_t, oop))
  //         CallRuntimeBarrierSet::call_runtime_store_in_heap_at<decorators,
  //         oop>;
  //     break;

  //   default:
  //     assert(false, "illegal basic type");
  //     break;
  // }
}

#endif  // OUR_PERSIST