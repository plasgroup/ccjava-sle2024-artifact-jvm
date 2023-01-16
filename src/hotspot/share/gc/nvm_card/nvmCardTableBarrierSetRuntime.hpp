#ifdef OUR_PERSIST

#pragma once

#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/shared/barrierSet.inline.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/accessDecorators.hpp"
#include "utilities/globalDefinitions.hpp"
// To call runtime functions from code stub
class NVMCardTableBarrierSetRuntime : public AllStatic {
 public:
  // template <DecoratorSet ds, typename T>
  static address write_nvm_field_post_entry(DecoratorSet decorators, BasicType type);
};

#endif  // OUR_PERSIST