#ifdef OUR_PERSIST

#include "oops/accessDecorators.hpp"
#include "oops/klass.hpp"

#ifndef NVM_NVMDEBUG_HPP
#define NVM_NVMDEBUG_HPP

class NVMDebug : AllStatic {
private:
public:
  static void print_decorators(DecoratorSet ds);
  static void print_klass_id(Klass* k);

  static bool cmp_dram_and_nvm_val(oop dram_obj, oop nvm_obj, ptrdiff_t offset,
                                   BasicType type, bool is_volatile);
  static bool is_same(oop dram_obj);
};

#endif // NVM_NVMDEBUG_HPP
#endif // OUR_PERSIST
