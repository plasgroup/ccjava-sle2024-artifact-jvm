#ifdef OUR_PERSIST

#ifndef NVM_NVMRECOVERY_HPP
#define NVM_NVMRECOVERY_HPP

#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"
#include "runtime/arguments.hpp"
#include "runtime/vmOperations.hpp"

class NVMRecovery : AllStatic {
 public:
  static int flag;
  static void main();
  static void test();

  // for getUserClassLoaders() method
 public:
  static jint get_loader(jobject obj_ary, TRAPS);
  static void collect_class_loader_during_gc();
};

class VM_OurPersistRecovery: public VM_Operation {
 private:
 public:
  VM_OurPersistRecovery() {}
  VMOp_Type type() const { return VMOp_OurPersistRecovery; }
  void doit();
};

#endif // NVM_NVMRECOVERY_HPP

#endif // OUR_PERSIST
