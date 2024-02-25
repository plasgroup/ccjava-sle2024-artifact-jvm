#ifdef OUR_PERSIST

#ifndef CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP
#define CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP

#include "asm/macroAssembler.hpp"
#include "gc/shared/cardTableBarrierSetAssembler.hpp"

class LIR_Assembler;
class StubAssembler;
class NVMCardTableWriteBarrierStub;
class NVMCardTableWriteBarrierAtomicStub;

class NVMCardTableBarrierSetAssembler: public CardTableBarrierSetAssembler {
 public:
  typedef CardTableBarrierSetAssembler Parent;
  typedef BarrierSetAssembler Raw;

  // entry point
  void store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                Address dst, Register val, Register tmp1, Register tmp2);
  void load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
               Register dst, Address src, Register tmp1, Register tmp_thread);

  // assembler
  void interpreter_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                            Address dst, Register val, Register tmp1, Register tmp2);
  void interpreter_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                Address dst, Register val, Register tmp1, Register tmp2);
  void interpreter_volatile_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                     Address dst, Register val, Register tmp1, Register tmp2);
  void interpreter_volatile_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                         Address dst, Register val, Register tmp1, Register tmp2);

  // utilities
  bool needs_wupd(DecoratorSet decorators, BasicType type);
  void writeback(MacroAssembler* masm, Address field, Register tmp = rscratch1);
  void lock_nvmheader(MacroAssembler* masm, Register base);
  void unlock_nvmheader(MacroAssembler* masm, Register base, Register tmp);
  // NOTE: set EFLAGS
  void load_nvm_fwd(MacroAssembler* masm, Register dst, Register base);
  // NOTE: set EFLAGS
  void is_target(MacroAssembler* masm, Register dst, Register base, Register tmp);
  void push_or_pop_all(MacroAssembler* masm, bool is_push, bool can_use_rdi, bool can_use_rsi,
                       Register tmp1 = noreg, Register tmp2 = noreg, Register tmp3 = noreg,
                       Register tmp4 = noreg, Register tmp5 = noreg, Register tmp6 = noreg,
                       Register tmp7 = noreg, Register tmp8 = noreg, Register tmp9 = noreg);

  // assertions
#ifdef ASSERT
  bool assert_sign_extended(uintptr_t mask);
#endif // ASSERT

  // call runtime
  void runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                        Address dst, Register val, Register tmp1, Register tmp2);
  void runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                       Register dst, Address src, Register tmp1, Register tmp_thread);
  void runtime_ensure_recoverable(MacroAssembler* masm, Register value,
                                                                 Register base,
                                                                 Register index);
  // NOTE: set EFLAGS
  void runtime_is_target(MacroAssembler* masm, Register dst, Register obj,
                         Register tmp1, Register tmp2, Register tmp3, Register tmp4);
  // NOTE: set EFLAGS
  void runtime_needs_wupd(MacroAssembler* masm, Register dst, Address obj,
                          DecoratorSet ds, bool is_oop,
                          Register tmp1, Register tmp2, Register tmp3, Register tmp4);
  #ifdef COMPILER1
  void generate_c1_write_barrier_runtime_stub(StubAssembler* sasm, DecoratorSet decorators, BasicType type) const;
  void generate_c1_write_barrier_atomic_runtime_stub(StubAssembler* sasm, DecoratorSet decorators, BasicType type) const;
  void gen_write_barrier_stub(LIR_Assembler* ce, NVMCardTableWriteBarrierStub* stub);
  void gen_write_barrier_atomic_stub(LIR_Assembler* ce, NVMCardTableWriteBarrierAtomicStub* stub);
  #endif
};

#endif // CPU_X86_GC_NVMCARD_NVMCARDTABLEBARRIERSETASSEMBLER_X86_HPP

#endif // OUR_PERSIST
