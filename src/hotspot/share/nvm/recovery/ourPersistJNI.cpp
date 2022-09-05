#ifdef OUR_PERSIST

#include "classfile/vmSymbols.hpp"
#include "jni.h"
#include "jvm.h"
#include "nvm/ourPersist.inline.hpp"
#include "nvm/recovery/nvmRecovery.hpp"
#include "nvm/recovery/ourPersistJNI.hpp"
#include "runtime/interfaceSupport.inline.hpp"
#include "runtime/thread.inline.hpp"
#include "utilities/exceptions.hpp"

JVM_ENTRY(static void, OurPersist_initNvmFile(JNIEnv *env, jclass clazz, jstring nvm_file_path))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::initNvmFile(env, clazz, nvm_file_path, CHECK);
JVM_END

JVM_ENTRY(static jboolean, OurPersist_hasEnableNvmData(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG_0(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  jboolean res = NVMRecovery::hasEnableNvmData(env, clazz, CHECK_0);
  return res;
JVM_END

JVM_ENTRY(static void, OurPersist_disableNvmData(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::disableNvmData(env, clazz, CHECK);
JVM_END

JVM_ENTRY(static void, OurPersist_initInternal(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::initInternal(env, clazz, CHECK);
JVM_END

JVM_ENTRY(static jobjectArray, OurPersist_nvmCopyClassNames(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG_NULL(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  jobjectArray res = NVMRecovery::nvmCopyClassNames(env, clazz, CHECK_NULL);
  return res;
JVM_END

JVM_ENTRY(static void, OurPersist_createDramCopy(JNIEnv *env, jclass clazz,
                                                 jobjectArray dram_copy_list, jobjectArray classes))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::createDramCopy(env, clazz, dram_copy_list, classes, CHECK);
JVM_END

JVM_ENTRY(static void, OurPersist_recoveryDramCopy(JNIEnv *env, jclass clazz,
                                                   jobjectArray dram_copy_list, jobjectArray classes))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::recoveryDramCopy(env, clazz, dram_copy_list, classes, CHECK);
JVM_END

JVM_ENTRY(static void, OurPersist_killMe(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  NVMRecovery::killMe(env, clazz, CHECK);
JVM_END

JVM_ENTRY(static jstring, OurPersist_mode(JNIEnv *env, jclass clazz))
  if (!OurPersist::enable()) {
    THROW_MSG_NULL(NVMRecovery::ourpersist_recovery_exception(), "OurPersist is disabled.");
  }
  jstring res = NVMRecovery::mode(env, clazz, CHECK_NULL);
  return res;
JVM_END

#define LANG "Ljava/lang/"
#define OBJ LANG "Object;"
#define STRING LANG "String;"
#define CLASS LANG "Class;"
#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

static JNINativeMethod ourpersist_methods[] = {
  {CC "initNvmFile", CC "(" STRING ")V", FN_PTR(OurPersist_initNvmFile)},
  {CC "hasEnableNvmData", CC "()Z", FN_PTR(OurPersist_hasEnableNvmData)},
  {CC "disableNvmData", CC "()V", FN_PTR(OurPersist_disableNvmData)},

  {CC "initInternal", CC "()V", FN_PTR(OurPersist_initInternal)},

  {CC "nvmCopyClassNames", CC "()[" STRING "", FN_PTR(OurPersist_nvmCopyClassNames)},
  {CC "createDramCopy", CC "([" OBJ "[" CLASS ")V", FN_PTR(OurPersist_createDramCopy)},
  {CC "recoveryDramCopy", CC "([" OBJ "[" CLASS ")V", FN_PTR(OurPersist_recoveryDramCopy)},

  {CC "killMe", CC "()V", FN_PTR(OurPersist_killMe)},
  {CC "mode", CC "()" STRING "", FN_PTR(OurPersist_mode)},
};

#undef LANG
#undef OBJ
#undef STRING
#undef CLASS
#undef CC
#undef FN_PTR

JVM_ENTRY(void, JVM_RegisterOurPersistMethods(JNIEnv *env, jclass ourpersist_class)) {
  ThreadToNativeFromVM ttnfv(thread);
  int ok = env->RegisterNatives(ourpersist_class, ourpersist_methods,
                                sizeof(ourpersist_methods)/sizeof(JNINativeMethod));
  guarantee(ok == 0, "register ourpersist natives");
} JVM_END

#endif // OUR_PERSIST
