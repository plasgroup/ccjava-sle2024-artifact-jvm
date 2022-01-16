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

  enum {
    our_persist_not_set,
    our_persist_enable,
    our_persist_disable
  };
  static int _enable;

 private:
  static void set_responsible_thread(void* nvm_ptr, Thread* cur_thread);
  static void clear_responsible_thread(Thread* cur_thread);

  static void copy_dram_to_nvm(oop from, oop to, ptrdiff_t offset, BasicType type, bool is_array = false);
  static bool cmp_dram_and_nvm(oop dram, oop nvm, ptrdiff_t offset, BasicType type, bool is_array = false);

  static void* allocate_nvm(int size, Thread* thr = NULL);

  static void copy_object(oop obj);
  //static void shade();

  #ifdef ASSERT
  inline static bool is_target_slow(Klass* klass);
#endif // ASSERT
  inline static bool is_target_fast(Klass* klass);

 public:
  inline static bool enable();
  inline static bool is_target(Klass* klass);
  inline static bool is_static_field(oop obj, ptrdiff_t offset);
  static Thread* responsible_thread(void* nvm_obj);
  static bool is_set_durableroot_annotation(oop klass_obj, ptrdiff_t offset);
  static bool is_volatile_and_non_mirror(oop obj, ptrdiff_t offset, DecoratorSet ds);

  static void ensure_recoverable(oop obj);
  static bool is_same(oop dram_object);
};

#endif // NVM_OURPERSIST_HPP

#endif // OUR_PERSIST
