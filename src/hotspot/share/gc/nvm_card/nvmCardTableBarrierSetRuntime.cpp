#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSetRuntime.hpp"

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"


address NVMCardTableBarrierSetRuntime::write_nvm_field_post_entry(DecoratorSet decorators, BasicType type) {
  assert((decorators == 537142272ULL) || (decorators == 537141312ULL) || (decorators == 537273344ULL) || (decorators == 538189888ULL) || (decorators == 805577728ULL) || (decorators == 1100317205504), " unknown decorator base");

  if (decorators == 537142272ULL) {
    if (type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jboolean>);
    else if (type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jchar>);
    else if (type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jfloat>);
    else if (type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jdouble>);
    else if (type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jbyte>);
    else if (type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jshort>);
    else if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537142272ULL, jlong>);
    else if (type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537142272ULL>);
    else if (type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537142272ULL>);
  } else if (decorators == 537141312ULL) {
    if (type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jboolean>);
    else if (type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jchar>);
    else if (type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jfloat>);
    else if (type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jdouble>);
    else if (type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jbyte>);
    else if (type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jshort>);
    else if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537141312ULL, jlong>);
    else if (type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537141312ULL>);
    else if (type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537141312ULL>);
  } else if (decorators == 537273344ULL) {
    if (type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jboolean>);
    else if (type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jchar>);
    else if (type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jfloat>);
    else if (type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jdouble>);
    else if (type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jbyte>);
    else if (type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jshort>);
    else if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<537273344ULL, jlong>);
    else if (type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537273344ULL>);
    else if (type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<537273344ULL>);
  } else if (decorators == 538189888ULL) {
    if (type == T_BOOLEAN) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jboolean>);
    else if (type == T_CHAR) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jchar>);
    else if (type == T_FLOAT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jfloat>);
    else if (type == T_DOUBLE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jdouble>);
    else if (type == T_BYTE) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jbyte>);
    else if (type == T_SHORT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jshort>);
    else if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_store_in_heap<538189888ULL, jlong>);
    else if (type == T_ARRAY) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<538189888ULL>);
    else if (type == T_OBJECT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_oop_store_in_heap<538189888ULL>);
  } else if (decorators == 805577728ULL) {
    // compare and swap
    if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_atomic_cmpxchg_in_heap<805577728ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_atomic_cmpxchg_in_heap<805577728ULL, jlong>);
  } else if (decorators == 1100317205504ULL) {
    if (type == T_INT) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_atomic_add_at_in_heap<1100317205504ULL, jint>);
    else if (type == T_LONG) return reinterpret_cast<address>(CallRuntimeBarrierSet::c1_call_runtime_atomic_add_at_in_heap<1100317205504ULL, jlong>);
  }

  
  printf("%s: shouldn't appear {decorator=%ld, type=%s}\n", __func__, decorators, type2name(type));
  ShouldNotReachHere();
  
  return 0;
  // assert(false, "wrong decorator or type");
}

#endif  // OUR_PERSIST