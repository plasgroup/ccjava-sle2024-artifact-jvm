#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSetRuntime.hpp"

#include "c1/c1_CodeStubs.hpp"
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "nvm/ourPersist.hpp"


address NVMCardTableBarrierSetRuntime::write_nvm_field_post_entry(DecoratorSet decorators, BasicType type) {
  return reinterpret_cast<address>(
      CallRuntimeBarrierSet::store_in_heap_at_ptr(decorators, type));
}

#endif  // OUR_PERSIST