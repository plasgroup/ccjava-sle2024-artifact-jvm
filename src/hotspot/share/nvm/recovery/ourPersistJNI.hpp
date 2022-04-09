
#ifdef OUR_PERSIST

#ifndef NVM_OURPERSISTJNI_HPP
#define NVM_OURPERSISTJNI_HPP

#include "jni.h"

extern "C" {
  void JNICALL JVM_RegisterOurPersistMethods(JNIEnv *env, jclass ourpersist_class);
}

#endif // NVM_OURPERSISTJNI_HPP

#endif // OUR_PERSIST
