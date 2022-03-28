#ifdef AUTO_PERSIST

#ifndef CPU_X86_GC_AUTOPERSIST_AUTOPERSISTBARRIERSETASSEMBLER_X86_HPP
#define CPU_X86_GC_AUTOPERSIST_AUTOPERSISTBARRIERSETASSEMBLER_X86_HPP

#include "asm/macroAssembler.hpp"
#include "gc/shared/cardTableBarrierSetAssembler.hpp"

class AutoPersistBarrierSetAssembler: public CardTableBarrierSetAssembler {
public:
  typedef CardTableBarrierSetAssembler Parent;
  typedef BarrierSetAssembler Raw;

  // entry point
  void store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                Address dst, Register val, Register tmp1, Register tmp2);
  void load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
               Register dst, Address src, Register tmp1, Register tmp_thread);
};

#endif // CPU_X86_GC_AUTOPERSIST_AUTOPERSISTBARRIERSETASSEMBLER_X86_HPP

#endif // AUTO_PERSIST
