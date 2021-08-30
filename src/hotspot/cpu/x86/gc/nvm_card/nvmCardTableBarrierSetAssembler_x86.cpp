#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interp_masm.hpp"
#include "nvmCardTableBarrierSetAssembler_x86.hpp"

#define __ ((InterpreterMacroAssembler*)masm)->

void NVMCardTableBarrierSetAssembler::runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                       Address dst, Register val, Register tmp1, Register tmp2) {
}

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp_thread && reg != dst)
void NVMCardTableBarrierSetAssembler::runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                      Register dst, Address src, Register tmp1, Register tmp_thread) {
}
#undef CHECK_PUSH_POP

void NVMCardTableBarrierSetAssembler::store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                               Address dst, Register val, Register tmp1, Register tmp2) {
  Parent::store_at(masm, decorators, type, dst, val, tmp1, tmp2);

  // DEBUG:
  bool is_set_static    = (decorators & OURPERSIST_IS_STATIC_MASK) != 0;
  bool is_set_volatile  = (decorators & OURPERSIST_IS_VOLATILE_MASK) != 0;
  if (!is_set_static)   printf("!is_set_static\n");
  if (!is_set_volatile) printf("!is_set_volatile\n");
  if (!is_set_static)   report_vm_error(__FILE__, __LINE__, "NVMCardTableBarrierSetAssembler::store_at");
}

void NVMCardTableBarrierSetAssembler::load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                              Register dst, Address src, Register tmp1, Register tmp_thread) {
  Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);

  // DEBUG:
  bool is_set_static    = (decorators & OURPERSIST_IS_STATIC_MASK) != 0;
  bool is_set_volatile  = (decorators & OURPERSIST_IS_VOLATILE_MASK) != 0;
  if (!is_set_static)   printf("!is_set_static\n");
  if (!is_set_volatile) printf("!is_set_volatile\n");
  if (!is_set_static)   report_vm_error(__FILE__, __LINE__, "NVMCardTableBarrierSetAssembler::load_at");
}

void NVMCardTableBarrierSetAssembler::put_field(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                Address dst, Register val, Register _tmp1, Register _tmp2) {
}

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2 && reg != tmp3 && reg != tmp4)
void NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(MacroAssembler* masm, Register val,
                                                                 Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
}
#undef CHECK_PUSH_POP

void NVMCardTableBarrierSetAssembler::oop_put_field(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                    Address dst, Register val, Register _tmp1, Register _tmp2) {
}

#endif // OUR_PERSIST
