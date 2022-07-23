
#ifdef OUR_PERSIST

#include "precompiled.hpp"
#include "jni.h"
#include "jvm.h"
#include "runtime/interfaceSupport.inline.hpp"
#include "nvm/recovery/nvmRecovery.hpp"

#include "oops/arrayOop.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/instanceOop.hpp"
#include "oops/klass.inline.hpp"
#include "oops/markWord.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/objArrayOop.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/jniHandles.inline.hpp"

#include "runtime/vmThread.hpp"
#include "runtime/vmOperations.hpp"
#include "classfile/vmSymbols.hpp"

JVM_ENTRY(static jobject, OurPersist_createDramCopyTest(JNIEnv *env, jobject clazz, jobject obj))
  ObjArrayKlass* k = NULL;
  int len = -1;
  {
    oop p = JNIHandles::resolve(obj);
    assert(oopDesc::is_oop(p), "");
    assert(p->klass()->is_objArray_klass(), "");
    k = ObjArrayKlass::cast(p->klass());
    len = objArrayOop(p)->length();
    assert(len >= 2, "");
  }

  jobject ary = obj;
  int test_loop = 1024*64;
  for (int i = 0; i < test_loop; i++) {
    for (int i = 0; i < len-1; i++) {
      oop elem = k->allocate(64, THREAD);
      objArrayOop p = objArrayOop(JNIHandles::resolve(ary));
      p->obj_at_put(i, elem);
    }

    if (i + 1 == test_loop) {
      objArrayOop(JNIHandles::resolve(ary))->obj_at_put(len-1, NULL);
      JNIHandles::destroy_local(ary);
    } else {
      objArrayOop next = k->allocate(len, THREAD);
      objArrayOop p = objArrayOop(JNIHandles::resolve(ary));
      p->obj_at_put(len-1, next);
      JNIHandles::destroy_local(ary);
      ary = JNIHandles::make_local(next);
    }
  }


  tty->print_cr("OurPersist_createDramCopyTest %s", k->external_name());
  return NULL;
JVM_END

JVM_ENTRY(static jobject, OurPersist_Test(JNIEnv *env, jobject clazz, jobject obj, jlong offset))
  // oop p = JNIHandles::resolve(obj);
  // assert_field_offset_sane(p, offset);
  // oop v = HeapAccess<ON_UNKNOWN_OOP_REF>::oop_load_at(p, offset);
  // return JNIHandles::make_local(THREAD, v);
  oop p = JNIHandles::resolve(clazz);
  Klass* k = p->klass();
  tty->print_cr("OurPersist_Test %s", k->external_name());
  return NULL;
JVM_END

JVM_ENTRY(static jobject, OurPersist_Test2(JNIEnv *env, jobject clazz, jlong offset))
  tty->print_cr("OurPersist_Test2 %ld", offset);
  oop p = JNIHandles::resolve(clazz);
  Klass* k = p->klass();
  tty->print_cr("OurPersist_Test2 %s", k->external_name());
  NVMRecovery::test();
  return NULL;
JVM_END

#include "classfile/javaClasses.hpp"
#include "utilities/exceptions.hpp"
#include "classfile/symbolTable.hpp"
JVM_ENTRY(static jint, OurPersist_Get_Klass_Names(JNIEnv *env, jobject clazz, jobject jni_obj_ary, jint start_pos))
  oop obj = JNIHandles::resolve(jni_obj_ary);
  if (obj == NULL) {
    THROW_MSG_0(SymbolTable::new_symbol("ourpersist/RecoveryException"), "obj == NULL");
  }
  if (!obj->is_objArray()) {
    THROW_MSG_0(SymbolTable::new_symbol("ourpersist/RecoveryException"), "!obj->is_objArray()");
  }
  objArrayOop obj_ary = objArrayOop(obj);

  jint pos = start_pos;
  int i = 0;
  for (; i < obj_ary->length() && pos < 15; i++, pos++) {
    char buf[256] = {0};
    sprintf(buf, "Klass_%d", pos);
    Handle h_test = java_lang_String::create_from_symbol(SymbolTable::new_symbol(buf), THREAD);
    obj_ary->obj_at_put(i, h_test());

    //Handle h = java_lang_String::create_from_symbol(obj_ary->klass()->name(), THREAD);
    //obj_ary->obj_at_put(i, h());

    //THROW_MSG_0(vmSymbols::java_lang_NullPointerException(), "test exception");
    //THROW_MSG_0(SymbolTable::new_symbol("ourpersist/RecoveryException"), "test exception");
  }

  return i;
JVM_END

//#include "services/heapDumper.hpp"
JVM_ENTRY(static void, OurPersist_Get_Loader(JNIEnv *env, jobject clazz, jobject ary_obj))
  NVMRecovery::get_loader(ary_obj, THREAD);
JVM_END

JVM_ENTRY(static jint, OurPersist_Get_User_Class_Loaders(JNIEnv *env, jobject clazz, jobject ary_obj))
  return NVMRecovery::get_loader(ary_obj, THREAD);
JVM_END


JVM_ENTRY(static void, OurPersist_Init(JNIEnv *env, jobject clazz, jobject ary_clds))
  VM_OurPersistRecoveryInit opr(ary_clds);
  VMThread::execute(&opr);
  tty->print_cr("OurPersist_Init");
  return;
JVM_END

#define LANG "Ljava/lang/"
#define OBJ LANG "Object;"
#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &f)

static JNINativeMethod ourpersist_methods[] = {
    {CC "init", CC "([Ljava/lang/ClassLoader;)V", FN_PTR(OurPersist_Init)},

    {CC "createDramCopyTest", CC "([" OBJ ")V", FN_PTR(OurPersist_createDramCopyTest)},
    {CC "test2", CC "(J)" OBJ "", FN_PTR(OurPersist_Test2)},
    {CC "test", CC "(" OBJ "J)" OBJ "", FN_PTR(OurPersist_Test)},
    {CC "getLoader", CC "([Ljava/lang/ClassLoader;)V", FN_PTR(OurPersist_Get_Loader)},

    {CC "getKlassNames", CC "([Ljava/lang/String;I)I", FN_PTR(OurPersist_Get_Klass_Names)},
    {CC "getUserClassLoaders", CC "([Ljava/lang/ClassLoader;)I", FN_PTR(OurPersist_Get_User_Class_Loaders)},
};

#undef LANG
#undef OBJ
#undef CC
#undef FN_PTR

JVM_ENTRY(void, JVM_RegisterOurPersistMethods(JNIEnv *env, jclass ourpersist_class)) {
  ThreadToNativeFromVM ttnfv(thread);
  int ok = env->RegisterNatives(ourpersist_class, ourpersist_methods,
                                sizeof(ourpersist_methods)/sizeof(JNINativeMethod));
  guarantee(ok == 0, "register ourpersist natives");
} JVM_END

#endif // OUR_PERSIST
