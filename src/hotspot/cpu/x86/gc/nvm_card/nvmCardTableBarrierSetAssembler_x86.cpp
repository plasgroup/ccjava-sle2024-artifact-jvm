#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "asm/macroAssembler.inline.hpp"
#include "interpreter/interp_masm.hpp"
#include "nvmCardTableBarrierSetAssembler_x86.hpp"
#include "nvm/callRuntimeBarrierSet.hpp"
#include "oops/nvmHeader.hpp"

#define __ ((InterpreterMacroAssembler*)masm)->

void NVMCardTableBarrierSetAssembler::store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                               Address dst, Register val, Register tmp1, Register tmp2) {
#ifdef OURPERSIST_STORE_RUNTIME_ONLY
  // Runtime
  NVMCardTableBarrierSetAssembler::runtime_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
#else  // OURPERSIST_STORE_RUNTIME_ONLY
  // implements volatile algorithm
  assert((decorators & OURPERSIST_IS_STATIC_MASK)   != DECORATORS_NONE, "");
  assert((decorators & OURPERSIST_IS_VOLATILE_MASK) != DECORATORS_NONE, "");
#ifndef OURPERSIST_IGNORE_VOLATILE
  if (decorators & OURPERSIST_IS_VOLATILE) {
    // Runtime
    // NVMCardTableBarrierSetAssembler::runtime_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    // OurPersist assembler
    if (is_reference_type(type)) {
      NVMCardTableBarrierSetAssembler::interpreter_volatile_oop_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    } else {
      NVMCardTableBarrierSetAssembler::interpreter_volatile_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    }
  } else {
    // OurPersist assembler
    if (is_reference_type(type)) {
      NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    } else {
      NVMCardTableBarrierSetAssembler::interpreter_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    }
  }
#else  // !OURPERSIST_IGNORE_VOLATILE
  // OurPersist assembler
  if (is_reference_type(type)) {
    //Parent::store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  } else {
    //Parent::store_at(masm, decorators, type, dst, val, tmp1, tmp2);
    NVMCardTableBarrierSetAssembler::interpreter_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  }
#endif // !OURPERSIST_IGNORE_VOLATILE
#endif // OURPERSIST_STORE_RUNTIME_ONLY
}

void NVMCardTableBarrierSetAssembler::load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                              Register dst, Address src, Register tmp1, Register tmp_thread) {
#ifdef OURPERSIST_LOAD_RUNTIME_ONLY
  // Runtime
  if ((decorators & OURPERSIST_IS_STATIC_MASK) == 0) {
    // TODO: TemplateInterpreterGenerator::generate_Reference_get_entry(void)
    Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  } else {
    NVMCardTableBarrierSetAssembler::runtime_load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
  }
#else  // OURPERSIST_LOAD_RUNTIME_ONLY
  // Original
  Parent::load_at(masm, decorators, type, dst, src, tmp1, tmp_thread);
#endif // OURPERSIST_LOAD_RUNTIME_ONLY
}

void NVMCardTableBarrierSetAssembler::interpreter_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                           Address dst, Register val, Register _tmp1, Register _tmp2) {
  Label done;
  Register tmp1 = r8;
  Register tmp2 = r9;

  // Store in DRAM.
  Parent::store_at(masm, decorators, type, dst, val, noreg, noreg);

  // fence
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
  // tmp1 = obj->nvm_header().fwd()
  NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp1, dst.base());

  // Check nvm header.
  __ jcc(Assembler::zero, done);

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    __ should_not_reach_here();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // Store in NVM.
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
  Raw::store_at(masm, decorators, type, nvm_field, val, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);

  __ bind(done);
}

void NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                               Address dst, Register val, Register _tmp1, Register _tmp2) {
  Label done_check_annotaion, val_is_null, done_set_val, done;
  Register tmp1 = r8;
  Register tmp2 = r9;

  // Check annotation
  assert((decorators & OURPERSIST_IS_STATIC_MASK) != 0, "");
  if ((val != noreg) && (decorators & OURPERSIST_IS_STATIC)) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, done_check_annotaion);

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
    // True
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, _tmp1, _tmp2, tmp1, tmp2);
    __ jmp(done_check_annotaion);
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    // False
    __ jmp(done_check_annotaion);
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

    // TODO:
    //  if (NVM::check_durableroot_annotation(obj, offset)) {
    //    make_object_recoverable(val);
    //  }
    __ unimplemented();

    __ bind(done_check_annotaion);
  }

  // Store in DRAM.
  __ movptr(tmp1, dst.base()); // push obj
  Parent::store_at(masm, decorators, type, dst, val, _tmp1, _tmp2);
  __ movptr(dst.base(), tmp1); // pop obj

  // fence
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
  // tmp1 = obj->nvm_header().fwd()
  NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp1, dst.base());
  __ jcc(Assembler::zero, done);
  // tmp1 = forwarding pointer

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    __ should_not_reach_here();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // if (value != NULL) OurPersist::ensure_recoverable(value);
  if (val != noreg) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, val_is_null);
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, noreg, noreg, noreg, noreg);
    __ bind(val_is_null);
  }

  // Store in NVM.
  // tmp1 = forwarding pointer
  // tmp2 = value
  __ xorl(tmp2, tmp2);
  if (val != noreg) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, done_set_val);

    // Check klass
    NVMCardTableBarrierSetAssembler::runtime_is_target(masm, tmp2, val, noreg, noreg, noreg, noreg);
    __ jcc(Assembler::zero, done_set_val);

    NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp2, val);
    __ bind(done_set_val);
  }
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
  Raw::store_at(masm, decorators, type, nvm_field, tmp2, _tmp1, _tmp2);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);

  __ bind(done);
}

#define PUSH_ALL_FOR_NVMCTBSA(RULE) \
  __ subq(rsp, 9 * wordSize); \
  RULE(rax) __ movq(Address(rsp, 8 * wordSize), rax); \
  RULE(rcx) __ movq(Address(rsp, 7 * wordSize), rcx); \
  RULE(rdx) __ movq(Address(rsp, 6 * wordSize), rdx); \
  RULE(rsi) __ movq(Address(rsp, 5 * wordSize), rsi); \
  RULE(rdi) __ movq(Address(rsp, 4 * wordSize), rdi); \
  RULE(r8)  __ movq(Address(rsp, 3 * wordSize),  r8); \
  RULE(r9)  __ movq(Address(rsp, 2 * wordSize),  r9); \
  RULE(r10) __ movq(Address(rsp, 1 * wordSize), r10); \
  RULE(r11) __ movq(Address(rsp, 0 * wordSize), r11);

#define POP_ALL_FOR_NVMCTBSA(RULE) \
  RULE(r11) __ movq(r11, Address(rsp, 0 * wordSize)); \
  RULE(r10) __ movq(r10, Address(rsp, 1 * wordSize)); \
  RULE(r9)  __ movq(r9,  Address(rsp, 2 * wordSize)); \
  RULE(r8)  __ movq(r8,  Address(rsp, 3 * wordSize)); \
  RULE(rdi) __ movq(rdi, Address(rsp, 4 * wordSize)); \
  RULE(rsi) __ movq(rsi, Address(rsp, 5 * wordSize)); \
  RULE(rdx) __ movq(rdx, Address(rsp, 6 * wordSize)); \
  RULE(rcx) __ movq(rcx, Address(rsp, 7 * wordSize)); \
  RULE(rax) __ movq(rax, Address(rsp, 8 * wordSize)); \
  __ addq(rsp, 9 * wordSize);

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2)
void NVMCardTableBarrierSetAssembler::runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                       Address dst, Register val, Register tmp1, Register tmp2) {
  // push all
  PUSH_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)

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

  // pop all
  POP_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)
}
#undef CHECK_PUSH_POP

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp_thread && reg != dst)
void NVMCardTableBarrierSetAssembler::runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                      Register dst, Address src, Register tmp1, Register tmp_thread) {
  if (type == T_LONG) {
    assert(dst == noreg, "");
    dst = rax;
  }

  // push all
  PUSH_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)

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

  // pop all
  POP_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)
}
#undef CHECK_PUSH_POP

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2 && reg != tmp3 && reg != tmp4)
void NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(MacroAssembler* masm, Register obj,
                                                                 Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
  assert(obj != noreg, "");

  // push all
  PUSH_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)

  // call
  address ensure_recoverable_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::ensure_recoverable_ptr());
  __ call_VM_leaf(ensure_recoverable_func, obj);

  // pop all
  POP_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)
}
#undef CHECK_PUSH_POP

#define CHECK_PUSH_POP(reg) if (reg != tmp1 && reg != tmp2 && reg != tmp3 && reg != tmp4 && reg != dst)
void NVMCardTableBarrierSetAssembler::runtime_is_target(MacroAssembler* masm, Register dst, Register obj,
                                                        Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
  assert(obj != noreg, "");

  // push all
  PUSH_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)

  // call
  address is_target_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::is_target_ptr());
  __ call_VM_leaf(is_target_func, obj);
  __ mov(dst, rax);

  // pop all
  POP_ALL_FOR_NVMCTBSA(CHECK_PUSH_POP)

  // mask result
  __ andl(dst, 0b1);
  // WARNING: Don't do anything after andptr instruction.
  // The status register is used in the following instructions.
}
#undef CHECK_PUSH_POP

void NVMCardTableBarrierSetAssembler::interpreter_volatile_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                                    Address dst, Register val, Register _tmp1, Register _tmp2) {
  Label dram_only;
  Register tmp1 = r8;
  Register tmp2 = r9;

  // lock
  NVMCardTableBarrierSetAssembler::lock_nvmheader(masm, dst.base(), tmp1, tmp2);

  // tmp1 = obj->nvm_header().fwd()
  NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp1, dst.base());

  // Check nvm header.
  __ jcc(Assembler::zero, dram_only);

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    __ should_not_reach_here();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // Store in NVM.
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
  Raw::store_at(masm, decorators, type, nvm_field, val, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);

  __ bind(dram_only);
  // Store in DRAM.
  Parent::store_at(masm, decorators, type, dst, val, noreg, noreg);

  // unlock
  NVMCardTableBarrierSetAssembler::unlock_nvmheader(masm, dst.base(), tmp1);
}

void NVMCardTableBarrierSetAssembler::interpreter_volatile_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                                        Address dst, Register val, Register _tmp1, Register _tmp2) {
  Label done_check_annotaion, val_is_null, done_set_val, dram_only;
  Register tmp1 = r8;
  Register tmp2 = r9;

  // Check annotation
  assert((decorators & OURPERSIST_IS_STATIC_MASK) != 0, "");
  if ((val != noreg) && (decorators & OURPERSIST_IS_STATIC)) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, done_check_annotaion);

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
    // True
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, _tmp1, _tmp2, tmp1, tmp2);
    __ jmp(done_check_annotaion);
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    // False
    __ jmp(done_check_annotaion);
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

    // TODO:
    //  if (NVM::check_durableroot_annotation(obj, offset)) {
    //    make_object_recoverable(val);
    //  }
    __ unimplemented();

    __ bind(done_check_annotaion);
  }

  // lock
  NVMCardTableBarrierSetAssembler::lock_nvmheader(masm, dst.base(), tmp1, tmp2);

  // fence
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
  // tmp1 = obj->nvm_header().fwd()
  NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp1, dst.base());
  __ jcc(Assembler::zero, dram_only);
  // tmp1 = forwarding pointer

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    __ should_not_reach_here();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // if (value != NULL) OurPersist::ensure_recoverable(value);
  if (val != noreg) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, val_is_null);
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, noreg, noreg, noreg, noreg);
    __ bind(val_is_null);
  }

  // Store in NVM.
  // tmp1 = forwarding pointer
  // tmp2 = value
  __ xorl(tmp2, tmp2);
  if (val != noreg) {
    __ cmpptr(val, 0);
    __ jcc(Assembler::equal, done_set_val);

    // Check klass
    NVMCardTableBarrierSetAssembler::is_target(masm, tmp2, val, noreg);
    __ jcc(Assembler::zero, done_set_val);

    NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp2, val);
    __ bind(done_set_val);
  }
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
  Raw::store_at(masm, decorators, type, nvm_field, tmp2, _tmp1, _tmp2);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);

  __ bind(dram_only);
  // Store in DRAM.
  __ movptr(tmp1, dst.base()); // for unlock
  Parent::store_at(masm, decorators, type, dst, val, _tmp1, _tmp2);

  // unlock
  NVMCardTableBarrierSetAssembler::unlock_nvmheader(masm, tmp1, tmp2);
}

// utilities
void NVMCardTableBarrierSetAssembler::writeback(MacroAssembler* masm, Address field, Register tmp) {
#ifdef ENABLE_NVM_WRITEBACK
#ifdef USE_CLWB
  __ lea(tmp, field);
  __ clwb(Address(tmp, 0));
#else
  __ clflush(field);
#endif
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
#endif
}

void NVMCardTableBarrierSetAssembler::lock_nvmheader(MacroAssembler* masm, Register base, Register _tmp1, Register _tmp2) {
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(nvmHeader::lock_mask_in_place), "");
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(~nvmHeader::lock_mask_in_place), "");

  Register tmp = noreg;
  Register tmp_for_stack = noreg;
  Label retry;
  const Address nvm_header(base, oopDesc::nvm_header_offset_in_bytes());

  // set temporary register
  assert(_tmp1 != _tmp2, "");
  if (_tmp1 == rax) {
    tmp = _tmp2;
  } else if (_tmp2 == rax) {
    tmp = _tmp1;
  } else {
    tmp = _tmp1;
    tmp_for_stack = _tmp2;
  }
  assert(tmp != noreg && tmp != rax, "");
  assert((_tmp1 != rax && _tmp2 != rax) || tmp_for_stack != noreg, "");

  // push if necessary
  if (tmp_for_stack != noreg) {
    __ mov(tmp_for_stack, rax);
  }

  // lock
  __ bind(retry);
  // CAS(nvm_header, cmpval = rax(...0), newval = tmp(...1)) --> rax
  __ movptr(tmp, nvm_header);
  __ movptr(rax, tmp);
  __ orptr(tmp, (int32_t)nvmHeader::lock_mask_in_place);
  __ andptr(rax, (int32_t)~nvmHeader::lock_mask_in_place);
  __ lock();
  __ cmpxchgptr(tmp, nvm_header);
  __ jcc(Assembler::notEqual, retry);

  // pop if necessary
  if (tmp_for_stack != noreg) {
    __ mov(rax, tmp_for_stack);
  }
}

void NVMCardTableBarrierSetAssembler::unlock_nvmheader(MacroAssembler* masm, Register base, Register tmp) {
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(nvmHeader::lock_mask_in_place), "");
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(~nvmHeader::lock_mask_in_place), "");

  // unlock
  const Address nvm_header(base, oopDesc::nvm_header_offset_in_bytes());
  __ movptr(tmp, nvm_header);
#ifdef ASSERT
  Label locked;
  __ testptr(tmp, (int32_t)nvmHeader::lock_mask_in_place);
  __ jcc(Assembler::notZero, locked);
  __ stop("unlocked.");
  __ bind(locked);
#endif // ASSERT
  __ andptr(tmp, (int32_t)~nvmHeader::lock_mask_in_place);
  __ movptr(nvm_header, tmp);
}

#ifdef ASSERT
bool NVMCardTableBarrierSetAssembler::assert_sign_extended(uintptr_t mask) {
  if ((mask & 0xffffffff00000000) != 0) {
    return (mask & (1 << 31)) != 0;
  } else {
    return (mask & (1 << 31)) == 0;
  }
  ShouldNotReachHere();
  return false;
}
#endif // ASSERT

void NVMCardTableBarrierSetAssembler::load_nvm_fwd(MacroAssembler* masm, Register dst, Register base) {
  assert(dst != base, "");
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(nvmHeader::fwd_mask_in_place), "");

  __ movptr(dst, Address(base, oopDesc::nvm_header_offset_in_bytes()));
  __ andptr(dst, (int32_t)nvmHeader::fwd_mask_in_place);
  // WARNING: Don't do anything after andptr instruction.
  // The status register is used in the following instructions.
}

void NVMCardTableBarrierSetAssembler::is_target(MacroAssembler* masm, Register dst, Register base, Register _tmp) {
  Label loop_super_klass, is_not_target, is_target, done;
  Register tmp = _tmp;
  Register sub_tmp = rbx;

  // push if necessary
  if (_tmp == noreg) {
    __ push(sub_tmp);
    tmp = sub_tmp;
  }
  assert_different_registers(dst, base, tmp);

  __ load_klass(dst, base, tmp);
  __ bind(loop_super_klass);
  __ movl(tmp, Address(dst, Klass::id_offset()));
  // dst = klass, tmp = KlassID

#ifdef ASSERT
  Label is_zero;
  uintptr_t mask_8bit_to_64bit = ~0b111;
  // 0 < KlassID < 6 (KLASS_ID_COUNT)
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(mask_8bit_to_64bit), "");
  __ testq(tmp, mask_8bit_to_64bit);
  __ jcc(Assembler::zero, is_zero);
  __ stop("sizeof(KlassID) = 4bytes, 0 < KlassID < 6. The upper bits should be zero.");
  __ bind(is_zero);
#endif // ASSERT

  // tmp = klass->id()
  __ cmp32(tmp, InstanceMirrorKlassID);
  __ jcc(Assembler::equal, is_not_target);
  __ cmp32(tmp, InstanceRefKlassID);
  __ jcc(Assembler::equal, is_not_target);
  __ cmp32(tmp, InstanceClassLoaderKlassID);
  __ jcc(Assembler::equal, is_not_target);

  // check super klass
  __ movptr(dst, Address(dst, Klass::super_offset()));
  __ cmpptr(dst, 0);
  __ jcc(Assembler::notZero, loop_super_klass);

  // is target
  __ movl(dst, 1);
  __ jmp(done);

  // is not target
  __ bind(is_not_target);
  __ xorl(dst, dst);

  __ bind(done);

#ifdef ASSERT
  Label is_same;
  NVMCardTableBarrierSetAssembler::runtime_is_target(masm, tmp, base, noreg, noreg, noreg, noreg);
  __ cmpq(tmp, dst);
  __ jcc(Assembler::equal, is_same);
  __ stop("is_target() failed.");
  __ bind(is_same);
#endif // ASSERT

  // pop if necessary
  if (_tmp == noreg) {
    __ pop(sub_tmp);
  }

  __ andl(dst, 0b1);
  // WARNING: Don't do anything after andptr instruction.
  // The status register is used in the following instructions.
}

#endif // OUR_PERSIST
