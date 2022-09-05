#ifdef OUR_PERSIST

#ifndef NVM_NVMRECOVERY_HPP
#define NVM_NVMRECOVERY_HPP

#include "oops/accessDecorators.hpp"
#include "oops/oopsHierarchy.hpp"
#include "runtime/arguments.hpp"
#include "runtime/vmOperations.hpp"
#include "classfile/symbolTable.hpp"

class NVMRecovery : AllStatic {
 friend class OurPersistJNI;
 friend class VM_OurPersistRecoveryDramCopy;
 friend class VM_OurPersistRecoveryInit;

 private:
  static Symbol* _ourpersist_recovery_exception;
  static bool _init_nvm;

  static void check_nvm_loaded(TRAPS);
  static Klass* nvmCopy2klass(nvmOop nvm_copy, TRAPS);
  static Klass* nvmMirrorCopy2klass(nvmMirrorOop nvm_mirror_copy, TRAPS);

 public:
  // DEBUG:
  static int create_mirror_count;

  static Symbol* ourpersist_recovery_exception() {
    assert(Thread::current() != NULL, "must be");
    if (_ourpersist_recovery_exception == NULL) {
      _ourpersist_recovery_exception = SymbolTable::new_symbol("ourpersist/RecoveryException");
    }
    return _ourpersist_recovery_exception;
  }

  static void initNvmFile(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS);
  static jboolean hasEnableNvmData(JNIEnv *env, jclass clazz, TRAPS);
  static void disableNvmData(JNIEnv *env, jclass clazz, TRAPS);
  static void initInternal(JNIEnv *env, jclass clazz, TRAPS);
  static void createNvmFile(JNIEnv *env, jclass clazz, TRAPS);
  static jobjectArray nvmCopyClassNames(JNIEnv *env, jclass clazz, TRAPS);
  static void createDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, TRAPS);
  static void recoveryDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, TRAPS);
  static void killMe(JNIEnv *env, jclass clazz, TRAPS);
  static jstring mode(JNIEnv *env, jclass clazz, TRAPS);
};

class VM_OurPersistRecoveryInit: public VM_Operation {
 private:
  JNIEnv* _env;
  jclass _clazz;
  jboolean _result;

 public:
  VM_OurPersistRecoveryInit(JNIEnv* env, jclass clazz):
    _env(env), _clazz(clazz), _result(JNI_FALSE) {}
  VMOp_Type type() const { return VMOp_OurPersistRecoveryInit; }
  jboolean result() { return _result; }
  void doit();
};

class VM_OurPersistRecoveryDramCopy: public VM_Operation {
 private:
  JNIEnv* _env;
  jclass _clazz;
  jobjectArray _dram_copy_list;
  jobjectArray _classes;
  jboolean _result;

 public:
  VM_OurPersistRecoveryDramCopy(JNIEnv* env, jclass clazz, jobjectArray dram_copy_list,
                                jobjectArray classes) :
                                  _env(env), _clazz(clazz), _dram_copy_list(dram_copy_list),
                                  _classes(classes), _result(JNI_FALSE) {}
  VMOp_Type type() const { return VMOp_OurPersistRecoveryDramCopy; }
  jboolean result() { return _result; }
  void doit();
};

#endif // NVM_NVMRECOVERY_HPP

#endif // OUR_PERSIST
