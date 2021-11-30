#ifdef OUR_PERSIST
#ifdef ASSERT

#include "oops/accessDecorators.hpp"
#include "oops/klass.hpp"

#ifndef NVM_NVMDEBUG_HPP
#define NVM_NVMDEBUG_HPP

class NVMDebug : AllStatic {
private:
public:
  static int obj_cnt;

  static void print_native_stack();
  static void print_decorators(DecoratorSet ds);
  static void print_klass_id(Klass* k);

  static bool cmp_dram_and_nvm_val(oop dram_obj, oop nvm_obj, ptrdiff_t offset,
                                   BasicType type, bool is_volatile);
  static bool cmp_dram_and_nvm_obj_during_gc(oop dram_obj);
};

#endif // NVM_NVMDEBUG_HPP
#endif // ASSERT
#endif // OUR_PERSIST
