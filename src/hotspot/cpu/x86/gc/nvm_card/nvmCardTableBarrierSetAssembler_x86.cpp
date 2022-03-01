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
  return;
#endif // OURPERSIST_STORE_RUNTIME_ONLY

  // Counter
  NVM_COUNTER_ONLY((NVMCounter::inc_access_asm(masm, true /* store */,
                                               ((decorators & OURPERSIST_IS_VOLATILE) != 0),
                                               is_reference_type(type),
                                               ((decorators & OURPERSIST_IS_STATIC) != 0),
                                               false /* interpreter */, false /* non-atomic */));)

  // implements volatile algorithm
  assert((decorators & OURPERSIST_IS_STATIC_MASK)   != DECORATORS_NONE, "");
  assert((decorators & OURPERSIST_IS_VOLATILE_MASK) != DECORATORS_NONE, "");
#ifndef OURPERSIST_IGNORE_VOLATILE
  if (decorators & OURPERSIST_IS_VOLATILE) {
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
  return;
#endif // !OURPERSIST_IGNORE_VOLATILE

  // OurPersist assembler
  if (is_reference_type(type)) {
    NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  } else {
    NVMCardTableBarrierSetAssembler::interpreter_store_at(masm, decorators, type, dst, val, tmp1, tmp2);
  }
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
  // FIXME:
  // Counter
  // NVM_COUNTER_ONLY((NVMCounter::inc_access_asm(masm, false /* load */,
  //                                              ((decorators & OURPERSIST_IS_VOLATILE) != 0),
  //                                              is_reference_type(type),
  //                                              ((decorators & OURPERSIST_IS_STATIC) != 0),
  //                                              false /* interpreter */));)

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
#ifndef NO_WUPD
  Raw::store_at(masm, decorators, type, nvm_field, val, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);
#endif // NO_WUPD

  __ bind(done);
}

void NVMCardTableBarrierSetAssembler::interpreter_oop_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                               Address dst, Register val, Register _tmp1, Register _tmp2) {
  Label done_check_annotaion, val_is_null, done_set_val, done;
  Register tmp1 = r8;
  Register tmp2 = r9;
  Register tmp3 = noreg;
  // WARNING: tmp4 is available after store.
  Register tmp4 = noreg;

  // set tmp3 and tmp4
  assert_different_registers(tmp1, tmp2, _tmp1, _tmp2, noreg);
  assert(dst.base() == _tmp1 || dst.index() == _tmp2, "");
  if (dst.base() == _tmp1) {
    assert_different_registers(_tmp1, _tmp2, dst.index());
    tmp3 = _tmp2;
    tmp4 = _tmp1;
  } else {
    assert(dst.index() == _tmp2, "");
    assert_different_registers(_tmp1, _tmp2, dst.base());
    tmp3 = _tmp1;
    tmp4 = _tmp2;
  }

  // Check annotation
  assert((decorators & OURPERSIST_IS_STATIC_MASK) != 0, "");
  if ((val != noreg) && (decorators & OURPERSIST_IS_STATIC)) {
    __ testptr(val, val);
    __ jcc(Assembler::zero, done_check_annotaion);

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
    // True
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, tmp1, tmp2, tmp3, noreg);
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
  __ movptr(tmp2, dst.base()); // save dst.base()
  Parent::store_at(masm, decorators, type, dst, val, tmp3, noreg); // dst.base() are broken

  // fence
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
  // tmp1 = obj->nvm_header().fwd()
  NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp1, tmp2);
  __ jcc(Assembler::zero, done);
  // tmp1 = forwarding pointer

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
    __ should_not_reach_here();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // if (value != NULL) OurPersist::ensure_recoverable(value);
  if (val != noreg) {
    __ testptr(val, val);
    __ jcc(Assembler::zero, val_is_null);
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, noreg, tmp2, tmp3, noreg);
    __ bind(val_is_null);
  }

  // Store in NVM.
  // tmp1 = forwarding pointer
  // tmp2 = value
  __ xorl(tmp2, tmp2);
  if (val != noreg) {
    __ testptr(val, val);
    __ jcc(Assembler::zero, done_set_val);

    // Check klass
    NVMCardTableBarrierSetAssembler::is_target(masm, tmp2, val, tmp3);
    __ jcc(Assembler::zero, done_set_val);

    NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp2, val);
    __ bind(done_set_val);
  }
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
#ifndef NO_WUPD
  Raw::store_at(masm, decorators, type, nvm_field, tmp2, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);
#endif // NO_WUPD

  __ bind(done);
}

void NVMCardTableBarrierSetAssembler::push_or_pop_all(MacroAssembler* masm, bool is_push,
                                                      bool can_use_rdi, bool can_use_rsi,
                                                      Register tmp1, Register tmp2, Register tmp3,
                                                      Register tmp4, Register tmp5, Register tmp6,
                                                      Register tmp7, Register tmp8, Register tmp9) {
  bool print_debug_msg = false;

  Register list[] = {rax, rcx, rdx, r8, r9}; // r10(rscratch1), r11(rscratch2)
  const int list_n = sizeof(list) / sizeof(Register);
  Register tmp[] = {tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7, tmp8, tmp9};
  const int tmp_n = sizeof(tmp) / sizeof(Register);

  int target_n = list_n;
  for (int i = 0; i < tmp_n; i++) {
    for (int j = 0; j < list_n; j++) {
      if (tmp[i] != noreg && tmp[i] == list[j]) {
        target_n--;
        list[j] = noreg;
        break;
      }
    }
  }

  Register scratch[] = {noreg, noreg, noreg}; // rdi, rsi, r12(r12_heapbase)
  int scratch_n = 0;
  if (can_use_rdi) {
    for (int i = 0; i < tmp_n; i++) {
      if (tmp[i] == rdi) {
        //scratch[scratch_n++] = rdi;
        break;
      }
    }
  }
  if (can_use_rsi) {
    for (int i = 0; i < tmp_n; i++) {
      if (tmp[i] == rsi) {
        //scratch[scratch_n++] = rsi;
        break;
      }
    }
  }
  if (!UseCompressedOops) {
    scratch[scratch_n++] = r12_heapbase;
  }

  bool only_with_scratch = target_n <= scratch_n;
  int mov_n = only_with_scratch ? target_n : scratch_n;
  int mem_n = only_with_scratch ? 0 : target_n - scratch_n;

  if (print_debug_msg) {
    tty->print_cr("push_or_pop_all: is_push=%d, target_n=%d, mov_n=%d, mem_n=%d",
      is_push, target_n, mov_n, mem_n);
    tty->print("tmp:");
    for (int i = 0; i < tmp_n; i++) {
      tty->print(" %s", tmp[i]->name());
    }
    tty->cr();
  }

  // save or restore with register
  if (mov_n != 0) {
    if (print_debug_msg) tty->print("mov(%s):", is_push ? "push" : "pop");
    int n = 0;
    for (int i = 0; n < mov_n && i < list_n; i++) {
      if (list[i] != noreg) {
        if (is_push) {
          __ movq(scratch[n], list[i]);
          if (print_debug_msg) tty->print(" %s <- %s", scratch[n]->name(), list[i]->name());
        } else {
          __ movq(list[i], scratch[n]);
          if (print_debug_msg) tty->print(" %s <- %s", list[i]->name(), scratch[n]->name());
        }
        list[i] = noreg;
        n++;
      }
    }
    assert(n == mov_n, "");
    if (print_debug_msg) tty->cr();
  }

  // save or restore with memory
  if (mem_n != 0) {
    if (is_push) {
      if (print_debug_msg) tty->print("push");
      int n = mem_n - 1;
      __ subq(rsp, mem_n * wordSize);
      for (int i = list_n - 1; 0 <= i; i--) {
        if (list[i] != noreg) {
          if (print_debug_msg) tty->print(" %s", list[i]->name());
          __ movq(Address(rsp, n * wordSize), list[i]);
          n--;
        }
      }
      assert(n == -1, "");
      if (print_debug_msg) tty->cr();
    } else {
      if (print_debug_msg) tty->print("pop");
      int n = 0;
      for (int i = 0; i < list_n; i++) {
        if (list[i] != noreg) {
          if (print_debug_msg) tty->print(" %s", list[i]->name());
          __ movq(list[i], Address(rsp, n * wordSize));
          n++;
        }
      }
      assert(n = mem_n, "");
      if (print_debug_msg) tty->cr();
      __ addq(rsp, mem_n * wordSize);
    }
  }
  if (print_debug_msg) tty->cr();
}

void NVMCardTableBarrierSetAssembler::runtime_store_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                       Address dst, Register val, Register tmp1, Register tmp2) {
  Register tmp3 = r8;
  Register tmp4 = r9;

  // push all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, true, false, false, tmp1, tmp2, tmp3, tmp4, dst.base(), dst.index(), val);

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
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, false, false, false, tmp1, tmp2, tmp3, tmp4, dst.base(), dst.index(), val);
}

void NVMCardTableBarrierSetAssembler::runtime_load_at(MacroAssembler* masm, DecoratorSet decorators, BasicType type,
                                                      Register dst, Address src, Register tmp1, Register tmp_thread) {
  Register tmp3 = r8;
  Register tmp4 = r9;

  if (type == T_LONG) {
    assert(dst == noreg, "");
    dst = rax;
  }

  // push all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, true, false, false, tmp1, tmp_thread, tmp3, tmp4, dst, src.base(), src.index());

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
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, false, false, false, tmp1, tmp_thread, tmp3, tmp4, dst, src.base(), src.index());
}

void NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(MacroAssembler* masm, Register obj,
                                                                 Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
  assert(obj != noreg, "");

  // push all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, true, false, false, tmp1, tmp2, tmp3, tmp4);

  // call
  address ensure_recoverable_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::ensure_recoverable_ptr());
  __ call_VM_leaf(ensure_recoverable_func, obj);

  // pop all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, false, false, false, tmp1, tmp2, tmp3, tmp4);
}

void NVMCardTableBarrierSetAssembler::runtime_is_target(MacroAssembler* masm, Register dst, Register obj,
                                                        Register tmp1, Register tmp2, Register tmp3, Register tmp4) {
  assert(obj != noreg, "");

  // push all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, true, false, false, tmp1, tmp2, tmp3, tmp4, dst);

  // call
  address is_target_func = CAST_FROM_FN_PTR(address, CallRuntimeBarrierSet::is_target_ptr());
  __ call_VM_leaf(is_target_func, obj);
  __ mov(dst, rax);

  // pop all
  NVMCardTableBarrierSetAssembler::push_or_pop_all(masm, false, false, false, tmp1, tmp2, tmp3, tmp4, dst);

  // mask result
  __ andl(dst, 0b1);
  // WARNING: Don't do anything after andptr instruction.
  // The status register is used in the following instructions.
}

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
#ifndef NO_WUPD
  Raw::store_at(masm, decorators, type, nvm_field, val, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);
#endif // NO_WUPD

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
  Register tmp3 = noreg;
  // WARNING: tmp4 is available after store.
  Register tmp4 = noreg;

  // set tmp3 and tmp4
  assert_different_registers(tmp1, tmp2, _tmp1, _tmp2, noreg);
  assert(dst.base() == _tmp1 || dst.index() == _tmp2, "");
  if (dst.base() == _tmp1) {
    assert_different_registers(_tmp1, _tmp2, dst.index());
    tmp3 = _tmp2;
    tmp4 = _tmp1;
  } else {
    assert(dst.index() == _tmp2, "");
    assert_different_registers(_tmp1, _tmp2, dst.base());
    tmp3 = _tmp1;
    tmp4 = _tmp2;
  }

  // Check annotation
  assert((decorators & OURPERSIST_IS_STATIC_MASK) != 0, "");
  if ((val != noreg) && (decorators & OURPERSIST_IS_STATIC)) {
    __ testptr(val, val);
    __ jcc(Assembler::zero, done_check_annotaion);

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
    // True
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, tmp1, tmp2, tmp3, noreg);
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
    __ testptr(val, val);
    __ jcc(Assembler::zero, val_is_null);
    NVMCardTableBarrierSetAssembler::runtime_ensure_recoverable(masm, val, noreg, tmp2, tmp3, noreg);
    __ bind(val_is_null);
  }

  // Store in NVM.
  // tmp1 = forwarding pointer
  // tmp2 = value
  __ xorl(tmp2, tmp2);
  if (val != noreg) {
    __ testptr(val, val);
    __ jcc(Assembler::zero, done_set_val);

    // Check klass
    NVMCardTableBarrierSetAssembler::is_target(masm, tmp2, val, tmp3);
    __ jcc(Assembler::zero, done_set_val);

    NVMCardTableBarrierSetAssembler::load_nvm_fwd(masm, tmp2, val);
    __ bind(done_set_val);
  }
  const Address nvm_field(tmp1, dst.index(), dst.scale(), dst.disp());
#ifndef NO_WUPD
  Raw::store_at(masm, decorators, type, nvm_field, tmp2, noreg, noreg);
  // Write back.
  NVMCardTableBarrierSetAssembler::writeback(masm, nvm_field, tmp2);
#endif // NO_WUPD

  __ bind(dram_only);
  // Store in DRAM.
  __ movptr(tmp1, dst.base()); // for unlock
  Parent::store_at(masm, decorators, type, dst, val, tmp3, noreg);

  // unlock
  NVMCardTableBarrierSetAssembler::unlock_nvmheader(masm, tmp1, tmp2);
}

// utilities
void NVMCardTableBarrierSetAssembler::writeback(MacroAssembler* masm, Address field, Register tmp) {
#ifdef ENABLE_NVM_WRITEBACK
#ifdef USE_CLWB
  if (field.index() != noreg || field.disp() != 0) {
    __ lea(tmp, field);
    field = Address(tmp, 0);
  }
  __ clwb(field);
  NVM_COUNTER_ONLY(NVMCounter::inc_clwb_asm(masm);)
#else
  __ clflush(field);
#endif
  __ membar(Assembler::Membar_mask_bits(Assembler::StoreLoad));
#endif
}

void NVMCardTableBarrierSetAssembler::lock_nvmheader(MacroAssembler* masm, Register base, Register _tmp1, Register _tmp2) {
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(nvmHeader::lock_mask_in_place), "");
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(~nvmHeader::lock_mask_in_place), "");
  assert_different_registers(base, rax);
  assert_different_registers(base, _tmp1, _tmp2);

  Register tmp = noreg;
  Register tmp_for_stack = noreg;
  Label retry;
  const Address nvm_header(base, oopDesc::nvm_header_offset_in_bytes());

  // set temporary register
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

#ifdef ASSERT
  const Address nvm_header_locked_thread(base, oopDesc::nvm_header_locked_offset_in_bytes());

  // check
  Label locked_thread_is_not_set;
  __ xorl(tmp, tmp);
  __ cmpptr(tmp, nvm_header_locked_thread);
  __ jcc(Assembler::equal, locked_thread_is_not_set);
  __ movptr(rax, nvm_header_locked_thread);
  __ stop("locked thread is already set. rax = nvm_header_locked_thread");
  __ bind(locked_thread_is_not_set);

  // set current thread
  __ movptr(nvm_header_locked_thread, r15_thread);
#endif // ASSERT

  // pop if necessary
  if (tmp_for_stack != noreg) {
    __ mov(rax, tmp_for_stack);
  }
}

void NVMCardTableBarrierSetAssembler::unlock_nvmheader(MacroAssembler* masm, Register base, Register tmp) {
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(nvmHeader::lock_mask_in_place), "");
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(~nvmHeader::lock_mask_in_place), "");
  assert_different_registers(base, tmp);

  const Address nvm_header(base, oopDesc::nvm_header_offset_in_bytes());

#ifdef ASSERT
  Label locked;
  __ movptr(tmp, nvm_header);
  __ testptr(tmp, (int32_t)nvmHeader::lock_mask_in_place);
  __ jcc(Assembler::notZero, locked);
  __ stop("unlocked.");
  __ bind(locked);

  const Address nvm_header_locked_thraed(base, oopDesc::nvm_header_locked_offset_in_bytes());
  Label locked_by_current_thread;
  __ cmpptr(r15_thread, nvm_header_locked_thraed);
  __ jcc(Assembler::equal, locked_by_current_thread);
  __ stop("locked by other thread.");
  __ bind(locked_by_current_thread);

  // clear nvm_header_locked_thraed
  __ xorl(tmp, tmp);
  __ movptr(nvm_header_locked_thraed, tmp);
#endif // ASSERT

  // unlock
  __ movptr(tmp, nvm_header);
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

void NVMCardTableBarrierSetAssembler::is_target(MacroAssembler* masm, Register dst, Register base, Register tmp) {
  Label is_not_target, is_target, done;
  assert_different_registers(dst, base, tmp);

  __ load_klass(dst, base, tmp);
  __ movl(tmp, Address(dst, Klass::id_offset()));
  // dst = klass, tmp = KlassID

#ifdef ASSERT
  Label assert_is_zero;
  uintptr_t mask_8bit_to_64bit = ~0b111;
  // 0 < KlassID < 6 (KLASS_ID_COUNT)
  assert(NVMCardTableBarrierSetAssembler::assert_sign_extended(mask_8bit_to_64bit), "");
  __ testq(tmp, mask_8bit_to_64bit);
  __ jcc(Assembler::zero, assert_is_zero);
  __ stop("sizeof(KlassID) = 4bytes, 0 < KlassID < 6. The upper bits should be zero.");
  __ bind(assert_is_zero);
#endif // ASSERT

  // tmp = klass->id()
  // assert(InstanceRefKlassID         == 1, "");
  // assert(InstanceMirrorKlassID      == 2, "");
  // assert(InstanceClassLoaderKlassID == 3, "");
  // if (tmp - 1 <= 2) --> is_not_target
  // __ subl(tmp, 1);
  // __ cmp32(tmp, 2);
  // __ jcc(Assembler::belowEqual, is_not_target);

  __ cmp32(tmp, InstanceMirrorKlassID);
  __ jcc(Assembler::equal, is_not_target);

  __ cmp32(tmp, InstanceClassLoaderKlassID);
  __ jcc(Assembler::equal, is_not_target);

  // is target
  __ bind(is_target);
  __ movl(dst, 1);
  __ jmp(done);

  // is not target
  __ bind(is_not_target);
  __ xorl(dst, dst);

  __ bind(done);

#ifdef ASSERT
  Label assert_is_same;
  NVMCardTableBarrierSetAssembler::runtime_is_target(masm, tmp, base, noreg, noreg, noreg, noreg);
  __ cmpq(tmp, dst);
  __ jcc(Assembler::equal, assert_is_same);
  __ stop("is_target() failed.");
  __ bind(assert_is_same);
#endif // ASSERT

  __ andl(dst, 0b1);
  // WARNING: Don't do anything after andptr instruction.
  // The status register is used in the following instructions.
}

#endif // OUR_PERSIST
