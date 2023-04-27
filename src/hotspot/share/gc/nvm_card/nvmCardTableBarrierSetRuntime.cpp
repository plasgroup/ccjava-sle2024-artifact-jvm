#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSetRuntime.hpp"

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"


address NVMCardTableBarrierSetRuntime::write_nvm_field_post_entry(DecoratorSet decorators, BasicType type) {
  
  if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jboolean>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jchar>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jfloat>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jdouble>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jbyte>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jshort>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jint>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE, jlong>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE>);
 if (decorators == (537141312ULL | OURPERSIST_IS_VOLATILE) && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<537141312ULL | OURPERSIST_IS_VOLATILE>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jboolean>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jchar>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jfloat>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jdouble>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jbyte>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jshort>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jint>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE, jlong>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE>);
 if (decorators == (537141312ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<537141312ULL | OURPERSIST_IS_NOT_VOLATILE>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jboolean>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jchar>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jfloat>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jdouble>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jbyte>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jshort>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jint>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE, jlong>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE>);
 if (decorators == (538189888ULL | OURPERSIST_IS_VOLATILE) && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<538189888ULL | OURPERSIST_IS_VOLATILE>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jboolean>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jchar>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jfloat>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jdouble>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jbyte>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jshort>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jint>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE, jlong>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE>);
 if (decorators == (538189888ULL | OURPERSIST_IS_NOT_VOLATILE) && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::call_runtime_oop_store_in_heap<538189888ULL | OURPERSIST_IS_NOT_VOLATILE>);

  printf("%s: shouldn't appear {decorator=%ld, type=%s}\n", __func__, decorators, type2name(type));
  return 0;
  // assert(false, "wrong decorator or type");
}

#endif  // OUR_PERSIST