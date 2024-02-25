#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_HPP
#define NVM_OURPERSIST_HPP

#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"
#include "runtime/arguments.hpp"
#include "nvm/oops/nvmOop.hpp"

class NVMWorkListStack;
class NVMBarrierSync;

class OurPersist : AllStatic {
 private:
  // used to global-lock-barrier-sync
  static unsigned long _copy_object_thread_count;

  enum {
    our_persist_unknown,
    our_persist_true,
    our_persist_false
  };
  static int _enable;
  static int _started;

 private:


  inline static bool cmp_dram_and_nvm(oop dram, oop nvm, ptrdiff_t offset, BasicType type, bool is_array = false);

  inline static nvmOop allocate_nvm(int size, Thread* thr = NULL);

  static void copy_object(oop obj);
  static void copy_object_thread_local(oop obj);
  static void copy_object_copy_step(oop obj, nvmOop nvm_obj, Klass* klass,
                                    NVMWorkListStack* worklist, NVMBarrierSync* barrier_sync,
                                    Thread* cur_thread);
  static void copy_object_copy_step_thread_local(oop obj, nvmOop nvm_obj, Klass* klass,
                                    NVMWorkListStack* worklist,
                                    Thread* cur_thread);

#ifdef ASSERT
  inline static bool is_target_slow(Klass* klass);
#endif // ASSERT
  inline static bool is_target_mirror_slow(Klass* klass);

  inline static bool is_target_fast(Klass* klass);
  inline static bool is_target_mirror_fast(Klass* klass);

  inline static bool enable_slow();

 public:
  static bool copy_object_verify_step(oop obj, nvmOop nvm_obj, Klass* klass);

  inline static void copy_dram_to_nvm(oop from, oop to, ptrdiff_t offset, BasicType type, bool is_array = false);

  inline static void clear_responsible_thread(Thread* cur_thread);
  inline static void add_dependent_obj_list(nvmOop nvm_obj, Thread* cur_thread);
  // static void make_persistent(Handle h_obj);
  static bool shade(oop obj, Thread* cur_thread);

  inline static bool enable();
  inline static bool started();
  inline static void set_started();
  inline static bool is_target(Klass* klass);
  inline static bool is_target_mirror(Klass* klass);
  inline static bool is_static_field(oop obj, ptrdiff_t offset);
  inline static bool is_volatile(oop obj, ptrdiff_t offset, DecoratorSet ds);
  inline static bool is_durableroot(oop klass_obj, ptrdiff_t offset, DecoratorSet ds);
  inline static bool needs_wupd(oop obj, ptrdiff_t offset, DecoratorSet ds, bool is_oop);

  static void ensure_recoverable_thread_local(oop obj);
  static void ensure_recoverable(oop obj);
  static void ensure_recoverable(Handle obj);
  static void handshake();

  static void mirror_create(Klass* klass, oop mirror);

  // TODO: move to new file
  inline static void copy_nvm_to_dram(nvmOop from, oop to, ptrdiff_t offset, BasicType type);
};

#endif // NVM_OURPERSIST_HPP

#endif // OUR_PERSIST
