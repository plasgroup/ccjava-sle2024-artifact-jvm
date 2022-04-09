#ifdef OUR_PERSIST

#ifndef NVM_NVMRECOVERYCLASSLOADERCOLLECTOR_HPP
#define NVM_NVMRECOVERYCLASSLOADERCOLLECTOR_HPP

#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"
#include "runtime/arguments.hpp"
#include "runtime/vmOperations.hpp"

class VM_OurPersistRecoveryGetLoader: public VM_Operation {
 private:
  jobject _loader_ary_obj;
  int _num_loaders;

 public:
  VM_OurPersistRecoveryGetLoader(jobject loader_ary_obj) :
    _loader_ary_obj(loader_ary_obj), _num_loaders(0) {}

  VMOp_Type type() const { return VMOp_OurPersistRecoveryGetLoader; }
  int num_loaders() { return _num_loaders; }
  void doit();
};

class NVMRecoveryClassLoaderCollector : AllStatic {
  // for getUserClassLoaders() method
 private:
  static jobject obj_ary_for_get_loader;
  static jint num_obj_ary_for_get_loader;
  static bool skip_builtin_loader;
  static void clear_obj_ary_for_get_loader() {
    obj_ary_for_get_loader = NULL;
    num_obj_ary_for_get_loader = 0;
  };
 public:
  static jint get_loader(jobject obj_ary);
  static void collect_class_loader_during_gc();
};

#endif // NVM_NVMRECOVERYCLASSLOADERCOLLECTOR_HPP

#endif // OUR_PERSIST
