#ifdef AUTO_PERSIST

#include "precompiled.hpp"
#include "gc/auto_persist/autoPersistBarrierSet.hpp"
#include "gc/auto_persist/autoPersistBarrierSetAssembler.hpp"
#include "gc/shared/cardTable.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#ifdef COMPILER1
#include "gc/shared/c1/cardTableBarrierSetC1.hpp"
#endif
#ifdef COMPILER2
#include "gc/shared/c2/cardTableBarrierSetC2.hpp"
#endif

AutoPersistBarrierSet::AutoPersistBarrierSet(CardTable* card_table) : CardTableBarrierSet(
          make_barrier_set_assembler<AutoPersistBarrierSetAssembler>(),
          make_barrier_set_c1<CardTableBarrierSetC1>(),
          make_barrier_set_c2<CardTableBarrierSetC2>(),
          card_table,
          BarrierSet::FakeRtti(BarrierSet::AutoPersistBarrierSet)) {
};

#endif // AUTO_PERSIST
