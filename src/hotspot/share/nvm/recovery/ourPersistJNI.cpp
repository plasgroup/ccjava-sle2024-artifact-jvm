#ifdef OUR_PERSIST

#include "jni.h"
#include "jvm.h"
#include "nvm/recovery/nvmRecovery.hpp"
#include "nvm/recovery/ourPersistJNI.hpp"
#include "runtime/interfaceSupport.inline.hpp"

JVM_ENTRY(static jboolean, OurPersist_exists(JNIEnv *env, jclass clazz, jstring nvm_file_path))
  return NVMRecovery::exists(env, clazz, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static void, OurPersist_initInternal(JNIEnv *env, jclass clazz, jstring nvm_file_path))
  NVMRecovery::initInternal(env, clazz, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static void, OurPersist_createNvmFile(JNIEnv *env, jclass clazz, jstring nvm_file_path))
  NVMRecovery::createNvmFile(env, clazz, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static jobjectArray, OurPersist_nvmCopyClassNames(JNIEnv *env, jclass clazz, jstring nvm_file_path))
  return NVMRecovery::nvmCopyClassNames(env, clazz, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static void, OurPersist_createDramCopy(JNIEnv *env, jclass clazz,
                                                 jobjectArray dram_copy_list, jobjectArray classes,
                                                 jstring nvm_file_path))
  NVMRecovery::createDramCopy(env, clazz, dram_copy_list, classes, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static void, OurPersist_recoveryDramCopy(JNIEnv *env, jclass clazz,
                                                   jobjectArray dram_copy_list, jobjectArray classes,
                                                   jstring nvm_file_path))
  NVMRecovery::recoveryDramCopy(env, clazz, dram_copy_list, classes, nvm_file_path, THREAD);
JVM_END

JVM_ENTRY(static void, OurPersist_killMe(JNIEnv *env, jclass clazz))
  NVMRecovery::killMe(env, clazz, THREAD);
JVM_END

#define LANG "Ljava/lang/"
#define OBJ LANG "Object;"
#define STRING LANG "String;"
#define CLASS LANG "Class;"
#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

static JNINativeMethod ourpersist_methods[] = {
  {CC "exists", CC "(" STRING ")Z", FN_PTR(OurPersist_exists)},

  {CC "initInternal", CC "(" STRING ")V", FN_PTR(OurPersist_initInternal)},
  {CC "createNvmFile", CC "(" STRING ")V", FN_PTR(OurPersist_createNvmFile)},

  {CC "nvmCopyClassNames", CC "(" STRING ")[" STRING "", FN_PTR(OurPersist_nvmCopyClassNames)},
  {CC "createDramCopy", CC "([" OBJ "[" CLASS "" STRING ")V", FN_PTR(OurPersist_createDramCopy)},
  {CC "recoveryDramCopy", CC "([" OBJ "[" CLASS "" STRING ")V", FN_PTR(OurPersist_recoveryDramCopy)},

  {CC "killMe", CC "()V", FN_PTR(OurPersist_killMe)},
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
