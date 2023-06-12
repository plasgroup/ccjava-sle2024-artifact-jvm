#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSetRuntime.hpp"

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"


address NVMCardTableBarrierSetRuntime::write_nvm_field_post_entry(DecoratorSet decorators, BasicType type) {
  assert((decorators == 537142272ULL) || (decorators == 537141312ULL) || (decorators == 537273344ULL) || (decorators == 538189888ULL) , " unknown decorator base");

  if (decorators == 537142272ULL && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jboolean>);
 if (decorators == 537142272ULL && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jchar>);
 if (decorators == 537142272ULL && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jfloat>);
 if (decorators == 537142272ULL && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jdouble>);
 if (decorators == 537142272ULL && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jbyte>);
 if (decorators == 537142272ULL && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jshort>);
 if (decorators == 537142272ULL && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jint>);
 if (decorators == 537142272ULL && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jlong>);
 if (decorators == 537142272ULL && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537142272ULL>);
 if (decorators == 537142272ULL && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537142272ULL>);
 if (decorators == 537141312ULL && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jboolean>);
 if (decorators == 537141312ULL && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jchar>);
 if (decorators == 537141312ULL && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jfloat>);
 if (decorators == 537141312ULL && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jdouble>);
 if (decorators == 537141312ULL && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jbyte>);
 if (decorators == 537141312ULL && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jshort>);
 if (decorators == 537141312ULL && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jint>);
 if (decorators == 537141312ULL && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jlong>);
 if (decorators == 537141312ULL && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537141312ULL>);
 if (decorators == 537141312ULL && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537141312ULL>);
 if (decorators == 537273344ULL && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jboolean>);
 if (decorators == 537273344ULL && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jchar>);
 if (decorators == 537273344ULL && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jfloat>);
 if (decorators == 537273344ULL && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jdouble>);
 if (decorators == 537273344ULL && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jbyte>);
 if (decorators == 537273344ULL && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jshort>);
 if (decorators == 537273344ULL && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jint>);
 if (decorators == 537273344ULL && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jlong>);
 if (decorators == 537273344ULL && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537273344ULL>);
 if (decorators == 537273344ULL && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537273344ULL>);
 if (decorators == 538189888ULL && type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jboolean>);
 if (decorators == 538189888ULL && type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jchar>);
 if (decorators == 538189888ULL && type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jfloat>);
 if (decorators == 538189888ULL && type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jdouble>);
 if (decorators == 538189888ULL && type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jbyte>);
 if (decorators == 538189888ULL && type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jshort>);
 if (decorators == 538189888ULL && type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jint>);
 if (decorators == 538189888ULL && type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jlong>);
 if (decorators == 538189888ULL && type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<538189888ULL>);
 if (decorators == 538189888ULL && type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<538189888ULL>);

  printf("%s: shouldn't appear {decorator=%ld, type=%s}\n", __func__, decorators, type2name(type));
  return 0;
  // assert(false, "wrong decorator or type");
}

#endif  // OUR_PERSIST