#ifdef OUR_PERSIST

#include "classfile/classLoaderDataGraph.inline.hpp"
#include "classfile/classLoaderExt.hpp"
#include "classfile/dictionary.hpp"
#include "classfile/symbolTable.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/vmSymbols.hpp"
#include "jvmci/compilerRuntime.hpp"
#include "nvm/oops/nvmMirrorOop.hpp"
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
#include "runtime/handles.inline.hpp"
#include "runtime/jniHandles.inline.hpp"
#include "runtime/thread.inline.hpp"
#include "runtime/vmOperations.hpp"
#include "runtime/vmThread.hpp"
#include "utilities/exceptions.hpp"
#include "utilities/macros.hpp"

Symbol* NVMRecovery::_ourpersist_recovery_exception = NULL;
// DEBUG:
int NVMRecovery::create_mirror_count = 0;

// TODO: implement
jboolean NVMRecovery::exists(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS) {
/*
  const char *nvm_file_path_cstr = env->GetStringUTFChars(nvm_file_path, NULL);
  if (nvm_file_path_cstr == NULL) {
    return JNI_FALSE;
  }
  // DEBUG:
  tty->print_cr("NVMRecovery::exists: %s", nvm_file_path_cstr);
*/
  //tty->print_cr("thread state: %d", THREAD->as_Java_thread()->thread_state());
  return JNI_TRUE;
}

void NVMRecovery::initInternal(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS) {
  VM_OurPersistRecoveryInit op(env, clazz, nvm_file_path);
  VMThread::execute(&op);
  if (op.result() == JNI_FALSE) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "Error in NVMRecovery::initInternal");
  }
}

// TODO: implement
void NVMRecovery::createNvmFile(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS) {
  THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "NVMRecovery::createNvmFile not implemented");
}

// TODO: implement
jobjectArray NVMRecovery::nvmCopyClassNames(JNIEnv *env, jclass clazz, jstring nvm_file_path, TRAPS) {
  THROW_MSG_NULL(NVMRecovery::ourpersist_recovery_exception(), "NVMRecovery::nvmCopyClassNames not implemented");
  return NULL;
}

// TODO: implement
void NVMRecovery::createDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, jstring nvm_file_path, TRAPS) {
  THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "NVMRecovery::createDramCopy not implemented");

  //Klass* a = Universe::intArrayKlassObj();
  //assert(a != NULL && a->is_typeArray_klass(), "sanity check");
  //TypeArrayKlass* tak = TypeArrayKlass::cast(a);
  //typeArrayOop tao = tak->allocate(10, THREAD);
  //assert(tao != NULL, "sanity check");
  //jobjectArray res = jobjectArray(JNIHandles::make_local(tao));
}

// TODO: implement
void NVMRecovery::recoveryDramCopy(JNIEnv *env, jclass clazz, jobjectArray dram_copy_list,
                             jobjectArray classes, jstring nvm_file_path, TRAPS) {
  THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "NVMRecovery::recoveryDramCopy not implemented");
}

void NVMRecovery::killMe(JNIEnv *env, jclass clazz, TRAPS) {
  //os::signal_raise(SIGTRAP);
  os::signal_raise(SIGKILL);
}

class OurPersistSetNvmMirrors : public KlassClosure {
 private:
  int _count;
  jboolean _result;
  int _nvm_mirror_n;
  nvmMirrorOop* _nvm_mirrors;

 public:
  OurPersistSetNvmMirrors(int nvm_mirrors_n, nvmMirrorOop nvm_mirrors[]) :
                          _count(0), _result(JNI_FALSE),
                          _nvm_mirror_n(nvm_mirrors_n), _nvm_mirrors(nvm_mirrors) {};
  int count() { return _count; }
  jboolean result() { return _result; }

  void do_klass(Klass* k) {
    assert(k != NULL, "");

    if (k->is_hidden()) {
      return;
    }

    oop mirror = k->java_mirror();
    assert(mirror != NULL, "");

    nvmMirrorOop nvm_mirror = nvmMirrorOop(mirror->nvm_header().fwd());
    if (nvm_mirror == NULL) {
      if (_nvm_mirrors != NULL) {
        char* klass_name = k->name()->as_C_string();
        for (int i = 0; i < _nvm_mirror_n; i++) {
          if (strcmp(klass_name, _nvm_mirrors[i]->klass_name()) == 0) {
            nvm_mirror = _nvm_mirrors[i];
            break;
          }
        }
      }
      if (nvm_mirror == NULL) {
        nvm_mirror = nvmMirrorOopDesc::create_mirror(k, mirror);
      }
      assert(nvm_mirror != NULL, "");

      nvmHeader::set_fwd(mirror, nvm_mirror);

      _count++; // DEBUG:
    }

    assert(k->java_mirror()->nvm_header().fwd() != NULL, "");
    _result = JNI_TRUE;
  }
};

void VM_OurPersistRecoveryInit::doit() {
  tty->print_cr("STW begin");
  //const char* path = _env->GetStringUTFChars(_nvm_file_path, NULL);
  //if (path == NULL) return;

  OurPersistSetNvmMirrors klass_closure(0, NULL);
  ClassLoaderDataGraph::classes_do(&klass_closure);
  if (klass_closure.result() == JNI_FALSE) return;

  OurPersist::set_started();

  _result = JNI_TRUE;
  tty->print_cr("STW end");
}

void VM_OurPersistRecoveryDramCopy::doit() {
  tty->print_cr("STW begin");
  tty->print_cr("STW end");
}

#endif // OUR_PERSIST
