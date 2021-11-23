#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"

#define OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, ctype, btype, is_store) { \
  if (is_store) { \
    if ((cmp_ds) == (ds) && type == btype) { \
      return (void*)(void(*)(oopDesc*, ptrdiff_t, ctype))CallRuntimeBarrierSet::call_runtime_store_in_heap_at<(ds), ctype>; \
    } \
  } else { \
    if ((cmp_ds) == (ds) && type == btype) { \
      return (void*)(ctype(*)(oopDesc*, ptrdiff_t))CallRuntimeBarrierSet::call_runtime_load_in_heap_at<(ds), ctype>; \
    } \
  } \
}
#define OURPERSIST_RUNTIME_BS_PTR_OOP_IF(cmp_ds, ds, ctype, btype, is_store) { \
  if (is_store) { \
    if ((cmp_ds) == (ds) && type == btype) { \
      return (void*)(void(*)(oopDesc*, ptrdiff_t, oopDesc*))CallRuntimeBarrierSet::call_runtime_oop_store_in_heap_at<(ds)>; \
    } \
  } else { \
    if ((cmp_ds) == (ds) && type == btype) { \
      return (void*)(ctype(*)(oopDesc*, ptrdiff_t))CallRuntimeBarrierSet::call_runtime_oop_load_in_heap_at<(ds)>; \
    } \
  } \
}
#define OURPERSIST_RUNTIME_BS_PTR_IF(cmp_ds, ds, is_store) { \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jbyte,    T_BYTE,    is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jchar,    T_CHAR,    is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jdouble,  T_DOUBLE,  is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jfloat,   T_FLOAT,   is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jint,     T_INT,     is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jlong,    T_LONG,    is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jshort,   T_SHORT,   is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jboolean, T_BOOLEAN, is_store) \
  OURPERSIST_RUNTIME_BS_PTR_PRIMITIVE_IF(cmp_ds, ds, jlong,    T_ADDRESS, is_store) \
  OURPERSIST_RUNTIME_BS_PTR_OOP_IF(cmp_ds, ds, oopDesc*, T_OBJECT, is_store) \
  OURPERSIST_RUNTIME_BS_PTR_OOP_IF(cmp_ds, ds, oopDesc*, T_ARRAY,  is_store) \
}

#define OURPERSIST_STORE_FUNC_PTR_IF_BASE(cmp_ds, ds) {\
  OURPERSIST_RUNTIME_BS_PTR_IF(cmp_ds, ds, true) \
}
#define OURPERSIST_STORE_FUNC_PTR_IF_2(cmp_ds, ds) {\
  OURPERSIST_STORE_FUNC_PTR_IF_BASE(cmp_ds, ds|OURPERSIST_IS_NOT_VOLATILE) \
  OURPERSIST_STORE_FUNC_PTR_IF_BASE(cmp_ds, ds|OURPERSIST_IS_VOLATILE) \
}
#define OURPERSIST_STORE_FUNC_PTR_IF_1(cmp_ds, ds) {\
  OURPERSIST_STORE_FUNC_PTR_IF_2(cmp_ds, ds|OURPERSIST_IS_NOT_STATIC) \
  OURPERSIST_STORE_FUNC_PTR_IF_2(cmp_ds, ds|OURPERSIST_IS_STATIC) \
}
#define OURPERSIST_STORE_FUNC_PTR_IF(cmp_ds, ds) {\
  OURPERSIST_STORE_FUNC_PTR_IF_1(cmp_ds, ds) \
}

#define OURPERSIST_LOAD_FUNC_PTR_IF_BASE(cmp_ds, ds) {\
  OURPERSIST_RUNTIME_BS_PTR_IF(cmp_ds, ds, false) \
}
#define OURPERSIST_LOAD_FUNC_PTR_IF_2(cmp_ds, ds) {\
  OURPERSIST_LOAD_FUNC_PTR_IF_BASE(cmp_ds, ds|OURPERSIST_IS_NOT_VOLATILE) \
  OURPERSIST_LOAD_FUNC_PTR_IF_BASE(cmp_ds, ds|OURPERSIST_IS_VOLATILE) \
}
#define OURPERSIST_LOAD_FUNC_PTR_IF_1(cmp_ds, ds) {\
  OURPERSIST_LOAD_FUNC_PTR_IF_2(cmp_ds, ds|OURPERSIST_IS_NOT_STATIC) \
  OURPERSIST_LOAD_FUNC_PTR_IF_2(cmp_ds, ds|OURPERSIST_IS_STATIC) \
}
#define OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, ds) {\
  OURPERSIST_LOAD_FUNC_PTR_IF_1(cmp_ds, ds) \
}

void* CallRuntimeBarrierSet::store_in_heap_at_ptr(DecoratorSet decorators, BasicType type) {
  assert(decorators & OURPERSIST_IS_VOLATILE_MASK, "");
  assert(decorators & OURPERSIST_IS_STATIC_MASK, "");

  DecoratorSet cmp_ds = decorators | OURPERSIST_BS_ASM;

  OURPERSIST_STORE_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|1318976ULL)
  OURPERSIST_STORE_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|270400ULL)
  OURPERSIST_STORE_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|270464ULL)

  report_vm_error(__FILE__, __LINE__, "Error:", "Should not reach here. decorators: %ld", cmp_ds);
  return NULL;
}

void* CallRuntimeBarrierSet::load_in_heap_at_ptr(DecoratorSet decorators, BasicType type) {
  assert(decorators & OURPERSIST_IS_VOLATILE_MASK, "");
  assert(decorators & OURPERSIST_IS_STATIC_MASK, "");

  DecoratorSet cmp_ds = decorators | OURPERSIST_BS_ASM;

  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|1318976ULL)
  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|270400ULL)
  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|270464ULL)
  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|303168ULL)
  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|532544ULL)
  OURPERSIST_LOAD_FUNC_PTR_IF(cmp_ds, OURPERSIST_BS_ASM|598080ULL)

  report_vm_error(__FILE__, __LINE__, "Error:", "Should not reach here. decorators: %ld", cmp_ds);
  return NULL;
}

void* CallRuntimeBarrierSet::ensure_recoverable_ptr() {
  return (void*)(void(*)(oopDesc*))CallRuntimeBarrierSet::call_runtime_ensure_recoverable;
}

void* CallRuntimeBarrierSet::is_target_ptr() {
  return (void*)(bool(*)(oopDesc*))CallRuntimeBarrierSet::call_runtime_is_target;
}

#endif // OUR_PERSIST
