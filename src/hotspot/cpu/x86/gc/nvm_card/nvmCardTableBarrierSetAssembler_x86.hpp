#ifdef OUR_PERSIST

#ifndef CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP
#define CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP

#include "asm/macroAssembler.hpp"
#include "gc/shared/cardTableBarrierSetAssembler.hpp"

class NVMCardTableBarrierSetAssembler: public CardTableBarrierSetAssembler {
public:
  typedef CardTableBarrierSetAssembler Parent;
  typedef BarrierSetAssembler Raw;

  // entry point
  void store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                Address dst, Register val, Register tmp1, Register tmp2);
  void load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
               Register dst, Address src, Register tmp1, Register tmp_thread);

  // call runtime
  void runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                        Address dst, Register val, Register tmp1, Register tmp2);
  void runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                       Register dst, Address src, Register tmp1, Register tmp_thread);
  void runtime_ensure_recoverable(MacroAssembler* masm, Register val,
                                  Register tmp1, Register tmp2, Register tmp3, Register tmp4);

  // assembler
  void put_field(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                 Address dst, Register val, Register tmp1, Register tmp2);
  void oop_put_field(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                     Address dst, Register val, Register tmp1, Register tmp2);
};

#endif // CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP

#endif // OUR_PERSIST
