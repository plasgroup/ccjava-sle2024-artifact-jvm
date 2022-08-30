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
 private:
  static Symbol* _ourpersist_recovery_exception;

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

  static jboolean exists(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS);
  static void initInternal(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS);
  static void createNvmFile(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS);
  static jobjectArray nvmCopyClassNames(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS);
  static void createDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, jstring nvm_file_path, TRAPS);
  static void recoveryDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, jstring nvm_file_path, TRAPS);
  static void killMe(JNIEnv *env, jclass clazz, TRAPS);
  static jstring mode(JNIEnv *env, jclass clazz, TRAPS);
};

class VM_OurPersistRecoveryInit: public VM_Operation {
 private:
  JNIEnv* _env;
  jclass _clazz;
  jstring _nvm_file_path;
  jboolean _result;

 public:
  VM_OurPersistRecoveryInit(JNIEnv* env, jclass clazz, jstring nvm_file_path):
    _env(env), _clazz(clazz), _nvm_file_path(nvm_file_path), _result(JNI_FALSE) {}
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
  jstring _nvm_file_path;
  jboolean _result;

 public:
  VM_OurPersistRecoveryDramCopy(JNIEnv* env, jclass clazz, jobjectArray dram_copy_list,
                                jobjectArray classes, jstring nvm_file_path) :
                                  _env(env), _clazz(clazz), _dram_copy_list(dram_copy_list),
                                  _classes(classes), _nvm_file_path(nvm_file_path), _result(JNI_FALSE) {}
  VMOp_Type type() const { return VMOp_OurPersistRecoveryDramCopy; }
  jboolean result() { return _result; }
  void doit();
};

#endif // NVM_NVMRECOVERY_HPP

#endif // OUR_PERSIST
