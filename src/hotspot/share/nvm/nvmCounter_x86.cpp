#ifdef NVM_COUNTER

#include "nvm/nvmCounter.hpp"
#include "asm/assembler.inline.hpp"
#include "asm/macroAssembler.hpp"
#include "interpreter/interpreter.hpp"
#include "interpreter/interp_masm.hpp"
#include "oops/klass.hpp"
#include "oops/method.hpp"

#define __ ((InterpreterMacroAssembler*)masm)->

#ifdef NVM_COUNTER_CHECK_DACAPO_RUN
void NVMCounter::set_dacapo_countable_flag(Klass* k, Method* m) {
  //if (NVMCounter::countable()) return;
  if (m->size_of_parameters() != 3) return;
  if (k->name()->equals("org/dacapo/harness/Callback") == false) return;

  tty->print_cr("[debug] ---");
  tty->print_cr("[debug] klass = %s", k->name()->as_C_string());
  tty->print_cr("[debug] method = %s", m->name()->as_C_string());
  tty->print_cr("[debug] arg size = %d", m->size_of_parameters());
  tty->print_cr("[debug] ---");

  if (m->name()->equals("start")) {
    NVMCounter::set_countable(true);
    tty->print_cr("[debug] set benchmark start flag");
  } else if (m->name()->equals("stop")) {
    NVMCounter::set_countable(false);
    tty->print_cr("[debug] set benchmark stop flag");
  }
}

void NVMCounter::set_dacapo_countable_flag_asm(MacroAssembler* masm, Register method, Register index, Register recv) {
  // push all
  __ push_d(xmm0); __ push_d(xmm1); __ push_d(xmm2); __ push_d(xmm3);
  __ push_d(xmm4); __ push_d(xmm5); __ push_d(xmm6); __ push_d(xmm7);
  __ pusha();

  // get receiver klass
  __ null_check(recv, oopDesc::klass_offset_in_bytes());
  Register tmp_load_klass = LP64_ONLY(rscratch1) NOT_LP64(noreg);
  __ load_klass(rax, recv, tmp_load_klass);
  // push args
  __ push(rax);
  // get target Method*
  __ lookup_virtual_method(rax, index, method);
  // push args
  __ push(method);

  // pop args and set register
  // arg2 = method
  __ pop(rax);
  __ movq(c_rarg1, rax);
  // arg1 = klass
  __ pop(rax);
  __ movq(c_rarg0, rax);
  // call
  address func = CAST_FROM_FN_PTR(address, NVMCounter::set_dacapo_countable_flag);
  __ call_VM_leaf(func, c_rarg0, c_rarg1);

  // pop all
  __ popa();
  __ pop_d(xmm7); __ pop_d(xmm6); __ pop_d(xmm5); __ pop_d(xmm4);
  __ pop_d(xmm3); __ pop_d(xmm2); __ pop_d(xmm1); __ pop_d(xmm0);
}
#endif // NVM_COUNTER_CHECK_DACAPO_RUN

void NVMCounter::inc_access_asm(MacroAssembler* masm, bool is_store, bool is_volatile, bool is_oop,
                                bool is_static, bool is_runtime, bool is_atomic) {
  // const Address counter(r15_thread, Thread::nvm_counter_offset());
  // __ movptr(tmp, counter);
  // const Address store_counter(tmp, offset_of(NVMCounter, _store));
  // __ incq(store_counter);

  __ pusha();
  address func = CAST_FROM_FN_PTR(address, ((void(*)(Thread*, int))NVMCounter::inc_access));
  int flags = NVMCounter::access_bool2flags(is_store, is_volatile, is_oop, is_static, is_runtime, is_atomic);
  __ movl(c_rarg1, flags);
  __ call_VM_leaf(func, r15_thread, c_rarg1);
  __ popa();
}

void NVMCounter::inc_access(Thread* thr, int flags) {
  thr->nvm_counter()->inc_access(flags);
}

#endif // NVM_COUNTER
