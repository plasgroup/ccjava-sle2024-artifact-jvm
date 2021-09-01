#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interp_masm.hpp"
#include "nvmCardTableBarrierSetAssembler_x86.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"

#define __ ((InterpreterMacroAssembler*)masm)->

void NVMCardTableBarrierSetAssembler::store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                               Address dst, Register val, Register tmp1, Register tmp2) {
  // Original assembler
  // Parent::store_at(masm, decorators, type, dst, val, tmp1, tmp2);

  // OurPersist assembler
  // if (is_reference_type(type)) {
  //   NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  // } else {
  //   NVMCardTableBarrierSetAssembler::interpreter_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  // }

  // Runtime
  NVMCardTableBarrierSetAssembler::runtime_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
}

void NVMCardTableBarrierSetAssembler::load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                              Register dst, Address src, Register tmp1, Register tmp_thread) {
  // Original
  // Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);

  // OurPersist assembler
  // if (is_reference_type(type)) {
  //   NVMCardTableBarrierSetAssembler::interpreter_oop_load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  // } else {
  //   NVMCardTableBarrierSetAssembler::interpreter_load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  // }

  // Runtime
  if ((decorators & OURPERSIST_IS_STATIC_MASK) == 0) {
    // TODO: TemplateInterpreterGenerator::generate_Reference_get_entry(void)
    Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  } else {
    NVMCardTableBarrierSetAssembler::runtime_load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  }
}

void NVMCardTableBarrierSetAssembler::interpreter_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                           Address dst, Register val, Register _tmp1, Register _tmp2) {
  // TODO:
}

void NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                               Address dst, Register val, Register _tmp1, Register _tmp2) {
  // TODO:
  // DEBUG:
  // if (val != noreg) {
  //   NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, noreg, noreg, noreg, noreg);
  // }
}

void NVMCardTableBarrierSetAssembler::interpreter_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                          Register dst, Address src, Register tmp1, Register tmp_thread) {
  // TODO:
}

void NVMCardTableBarrierSetAssembler::interpreter_oop_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                              Register dst, Address src, Register tmp1, Register tmp_thread) {
  // TODO:
}

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2)
void NVMCardTableBarrierSetAssembler::runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                       Address dst, Register val, Register tmp1, Register tmp2) {
  // push
  __ subq(rsp, 9 * wordSize);
  CHECK_PUSH_POP(rax) __ movq(Address(rsp, 8 * wordSize), rax);
  CHECK_PUSH_POP(rcx) __ movq(Address(rsp, 7 * wordSize), rcx);
  CHECK_PUSH_POP(rdx) __ movq(Address(rsp, 6 * wordSize), rdx);
  CHECK_PUSH_POP(rsi) __ movq(Address(rsp, 5 * wordSize), rsi);
  CHECK_PUSH_POP(rdi) __ movq(Address(rsp, 4 * wordSize), rdi);
  CHECK_PUSH_POP(r8)  __ movq(Address(rsp, 3 * wordSize), r8);
  CHECK_PUSH_POP(r9)  __ movq(Address(rsp, 2 * wordSize), r9);
  CHECK_PUSH_POP(r10) __ movq(Address(rsp, 1 * wordSize), r10);
  CHECK_PUSH_POP(r11) __ movq(Address(rsp, 0 * wordSize), r11);

  // arg1 = obj
  if (c_rarg0 != dst.base()) {
    __ mov(c_rarg0, dst.base());
  }

  assert(c_rarg0 != dst.index(), "");
  // arg2 = offset = index * scale_size + disp
  __ xorl(c_rarg1, c_rarg1);
  Address offset(c_rarg1 /* zero */, dst.index(), dst.scale(), dst.disp());
  __ lea(c_rarg1, offset);

  // arg3 = value
  assert(c_rarg0 != val, "");
  assert(c_rarg1 != val, "");
  // set value
  if (!is_floating_point_type(type)) {
    if (is_reference_type(type) && val == noreg) {
      __ xorl(c_rarg2, c_rarg2);
    } else if (type == T_LONG) {
      __ mov(c_rarg2, rax);
    } else if (c_rarg2 != val) {
      __ mov(c_rarg2, val);
    }
  }
  // mask
  if (type == T_BOOLEAN) {
    __ andl(c_rarg2, 0x1);
  }

  // call
  address store_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::store_in_heap_at_ptr(decorators, type));
  if (is_floating_point_type(type)) {
    __ call_VM_leaf(store_func, c_rarg0, c_rarg1 /*, xmm0 */);
  } else {
    __ call_VM_leaf(store_func, c_rarg0, c_rarg1, c_rarg2);
  }

  // pop
  CHECK_PUSH_POP(r11) __ movq(r11, Address(rsp, 0 * wordSize));
  CHECK_PUSH_POP(r10) __ movq(r10, Address(rsp, 1 * wordSize));
  CHECK_PUSH_POP(r9)  __ movq(r9,  Address(rsp, 2 * wordSize));
  CHECK_PUSH_POP(r8)  __ movq(r8,  Address(rsp, 3 * wordSize));
  CHECK_PUSH_POP(rdi) __ movq(rdi, Address(rsp, 4 * wordSize));
  CHECK_PUSH_POP(rsi) __ movq(rsi, Address(rsp, 5 * wordSize));
  CHECK_PUSH_POP(rdx) __ movq(rdx, Address(rsp, 6 * wordSize));
  CHECK_PUSH_POP(rcx) __ movq(rcx, Address(rsp, 7 * wordSize));
  CHECK_PUSH_POP(rax) __ movq(rax, Address(rsp, 8 * wordSize));
  __ addq(rsp, 9 * wordSize);
}
#undef CHECK_PUSH_POP

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp_thread && reg != dst)
void NVMCardTableBarrierSetAssembler::runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                      Register dst, Address src, Register tmp1, Register tmp_thread) {
  if (type == T_LONG) {
    assert(dst == noreg, "");
    dst = rax;
  }

  // push
  __ subq(rsp, 9 * wordSize);
  CHECK_PUSH_POP(rax) __ movq(Address(rsp, 8 * wordSize), rax);
  CHECK_PUSH_POP(rcx) __ movq(Address(rsp, 7 * wordSize), rcx);
  CHECK_PUSH_POP(rdx) __ movq(Address(rsp, 6 * wordSize), rdx);
  CHECK_PUSH_POP(rsi) __ movq(Address(rsp, 5 * wordSize), rsi);
  CHECK_PUSH_POP(rdi) __ movq(Address(rsp, 4 * wordSize), rdi);
  CHECK_PUSH_POP(r8)  __ movq(Address(rsp, 3 * wordSize), r8);
  CHECK_PUSH_POP(r9)  __ movq(Address(rsp, 2 * wordSize), r9);
  CHECK_PUSH_POP(r10) __ movq(Address(rsp, 1 * wordSize), r10);
  CHECK_PUSH_POP(r11) __ movq(Address(rsp, 0 * wordSize), r11);

  // arg1 = obj
  if (c_rarg0 != src.base()) {
    __ mov(c_rarg0, src.base());
  }

  assert(c_rarg0 != src.index(), "");
  // arg2 = offset = index * scale_size + disp
  __ xorl(c_rarg1, c_rarg1);
  Address offset(c_rarg1 /* zero */, src.index(), src.scale(), src.disp());
  __ lea(c_rarg1, offset);

  // call
  address load_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::load_in_heap_at_ptr(decorators, type));
  __ call_VM_leaf(load_func, c_rarg0, c_rarg1);
  // dst = result, long: rax = result, float or double: xmm0 = result
  switch (type) {
    case T_BOOLEAN: __ movzbl(dst, rax); break;
    case T_BYTE:    __ movsbl(dst, rax); break;
    case T_CHAR:    __ movzwl(dst, rax); break;
    case T_SHORT:   __ movswl(dst, rax); break;
    case T_INT:     __ movl  (dst, rax); break;
    case T_ADDRESS: __ movptr(dst, rax); break;
    case T_FLOAT:   __ movflt(xmm0, xmm0); break;
    case T_DOUBLE:  __ movdbl(xmm0, xmm0); break;
    case T_LONG:    __ movq  (dst, rax); break;
    case T_OBJECT:
    case T_ARRAY:   __ movptr(dst, rax); break;
    default: ShouldNotReachHere(); break;
  }

  // pop
  CHECK_PUSH_POP(r11) __ movq(r11, Address(rsp, 0 * wordSize));
  CHECK_PUSH_POP(r10) __ movq(r10, Address(rsp, 1 * wordSize));
  CHECK_PUSH_POP(r9)  __ movq(r9,  Address(rsp, 2 * wordSize));
  CHECK_PUSH_POP(r8)  __ movq(r8,  Address(rsp, 3 * wordSize));
  CHECK_PUSH_POP(rdi) __ movq(rdi, Address(rsp, 4 * wordSize));
  CHECK_PUSH_POP(rsi) __ movq(rsi, Address(rsp, 5 * wordSize));
  CHECK_PUSH_POP(rdx) __ movq(rdx, Address(rsp, 6 * wordSize));
  CHECK_PUSH_POP(rcx) __ movq(rcx, Address(rsp, 7 * wordSize));
  CHECK_PUSH_POP(rax) __ movq(rax, Address(rsp, 8 * wordSize));
  __ addq(rsp, 9 * wordSize);
}
#undef CHECK_PUSH_POP

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2 && reg != tmp3 && reg != tmp4)
void NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(MacroAssembler* masm, Register obj,
                                                                 Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
  assert(obj != noreg, "");

  // push
  __ subq(rsp, 9 * wordSize);
  CHECK_PUSH_POP(rax) __ movq(Address(rsp, 8 * wordSize), rax);
  CHECK_PUSH_POP(rcx) __ movq(Address(rsp, 7 * wordSize), rcx);
  CHECK_PUSH_POP(rdx) __ movq(Address(rsp, 6 * wordSize), rdx);
  CHECK_PUSH_POP(rsi) __ movq(Address(rsp, 5 * wordSize), rsi);
  CHECK_PUSH_POP(rdi) __ movq(Address(rsp, 4 * wordSize), rdi);
  CHECK_PUSH_POP(r8)  __ movq(Address(rsp, 3 * wordSize), r8);
  CHECK_PUSH_POP(r9)  __ movq(Address(rsp, 2 * wordSize), r9);
  CHECK_PUSH_POP(r10) __ movq(Address(rsp, 1 * wordSize), r10);
  CHECK_PUSH_POP(r11) __ movq(Address(rsp, 0 * wordSize), r11);

  // call
  address ensure_recoverable_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::ensure_recoverable_ptr());
  __ call_VM_leaf(ensure_recoverable_func, obj);

  // pop
  CHECK_PUSH_POP(r11) __ movq(r11, Address(rsp, 0 * wordSize));
  CHECK_PUSH_POP(r10) __ movq(r10, Address(rsp, 1 * wordSize));
  CHECK_PUSH_POP(r9)  __ movq(r9,  Address(rsp, 2 * wordSize));
  CHECK_PUSH_POP(r8)  __ movq(r8,  Address(rsp, 3 * wordSize));
  CHECK_PUSH_POP(rdi) __ movq(rdi, Address(rsp, 4 * wordSize));
  CHECK_PUSH_POP(rsi) __ movq(rsi, Address(rsp, 5 * wordSize));
  CHECK_PUSH_POP(rdx) __ movq(rdx, Address(rsp, 6 * wordSize));
  CHECK_PUSH_POP(rcx) __ movq(rcx, Address(rsp, 7 * wordSize));
  CHECK_PUSH_POP(rax) __ movq(rax, Address(rsp, 8 * wordSize));
  __ addq(rsp, 9 * wordSize);
}
#undef CHECK_PUSH_POP

#endif // OUR_PERSIST
