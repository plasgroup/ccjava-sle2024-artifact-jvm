#ifdef AUTO_PERSIST

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interp_masm.hpp"
#include "autoPersistBarrierSetAssembler_x86.hpp"

#define __ ((InterpreterMacroAssembler*)masm)->

void AutoPersistBarrierSetAssembler::store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                               Address dst, Register val, Register tmp1, Register tmp2) {
  // fence
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
  // NOTE: r12_heapbase isn't used when compressed-oops is enabled.
  assert(!UseCompressedOops, "not supported");
  __ movptr(r12_heapbase, Address(dst.base(), oopDesc::autopersist_nvm_header_offset_in_bytes()));

  Parent::store_at(masm, decorators, type, dst, val, tmp1, tmp2);
}

void AutoPersistBarrierSetAssembler::load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                              Register dst, Address src, Register tmp1, Register tmp_thread) {
  // NOTE: r12_heapbase isn't used when compressed-oops is enabled.
  assert(!UseCompressedOops, "not supported");
  __ movptr(r12_heapbase, Address(src.base(), oopDesc::autopersist_nvm_header_offset_in_bytes()));
  Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
}

#endif // AUTO_PERSIST
