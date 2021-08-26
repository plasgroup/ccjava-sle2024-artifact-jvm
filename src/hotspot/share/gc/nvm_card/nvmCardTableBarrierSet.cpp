#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSetAssembler.hpp"
#include "gc/shared/cardTable.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#ifdef COMPILER1
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#endif
#ifdef COMPILER2
#include "gc/shared/c2/cardTableBarrierSetC2.hpp"
#endif

NVMCardTableBarrierSet::NVMCardTableBarrierSet(CardTable* card_table) : CardTableBarrierSet(
          make_barrier_set_assembler<NVMCardTableBarrierSetAssembler>(),
          make_barrier_set_c1<CardTableBarrierSetC1>(),
          make_barrier_set_c2<CardTableBarrierSetC2>(),
          card_table,
          BarrierSet::FakeRtti(BarrierSet::NVMCardTableBarrierSet)) {
};

#endif // OUR_PERSIST
