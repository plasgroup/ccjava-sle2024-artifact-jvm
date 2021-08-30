#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_HPP
#define NVM_OURPERSIST_HPP

#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"
#include "runtime/arguments.hpp"

class OurPersist : AllStatic {
 private:
  // used to global-lock-barrier-sync
  static unsigned long _copy_object_thread_count;
  // check -Xint and -XX:-UseCompressedOops option.
  static bool _enable;
  static bool _enable_is_set;

 private:
  static Thread* responsible_thread(void* nvm_obj);
  static void set_responsible_thread(void* nvm_ptr, Thread* cur_thread);
  static void clear_responsible_thread(Thread* cur_thread);

  static bool check_durableroot_annotation(oop klass_obj, ptrdiff_t offset);

  static void copy_dram_to_nvm(oop from, oop to, ptrdiff_t offset, BasicType type, bool is_array = false);
  static bool cmp_dram_and_nvm(oop dram, oop nvm, ptrdiff_t offset, BasicType type, bool is_array = false);

  static bool is_volatile(oop obj, ptrdiff_t offset, DecoratorSet ds);
  static bool is_volatile_fast(oop obj, ptrdiff_t offset, DecoratorSet ds);
  static bool is_volatile_slow(oop obj, ptrdiff_t offset, DecoratorSet ds);

  static void* allocate_nvm(int size, Thread* thr = NULL);

  static void copy_object(oop obj);
  //static void shade();

 public:
  static bool enable() {
    if (!_enable_is_set) {
      _enable = Arguments::is_interpreter_only() && (!UseCompressedOops);
      _enable_is_set = true;
    }
    return _enable;
  }

  static void ensure_recoverable(oop obj);
};

#endif // NVM_OURPERSIST_HPP

#endif // OUR_PERSIST
