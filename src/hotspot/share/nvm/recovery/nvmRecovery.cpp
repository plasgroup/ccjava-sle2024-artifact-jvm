#ifdef OUR_PERSIST

#include "classfile/systemDictionary.hpp"
#include "classfile/systemDictionaryShared.hpp"
#include "classfile/vmSymbols.hpp"

#include "oops/arrayOop.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/instanceOop.hpp"
#include "oops/klass.inline.hpp"
#include "oops/markWord.hpp"
#include "oops/method.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/objArrayOop.inline.hpp"
#include "oops/oop.inline.hpp"

#include "nvm/recovery/nvmRecovery.hpp"

#include "utilities/exceptions.hpp"
#include "utilities/macros.hpp"

#include "jvmci/compilerRuntime.hpp"
#include "classfile/classLoaderDataGraph.inline.hpp"

#include "classfile/dictionary.hpp"

#include "runtime/handles.inline.hpp"

#include "classfile/classLoaderExt.hpp"

#include "runtime/jniHandles.inline.hpp"

// DEBUG:
int NVMRecovery::create_mirror_count = 0;

class CLDClosureTest : public CLDClosure {
 public:
  Klass* _klass;
  Thread* _thread;
  Symbol* _sym;
  ClassLoaderData* _debug;

  CLDClosureTest(Thread* thread, Symbol* sym): _klass(NULL), _thread(thread), _sym(sym), _debug(NULL) {}

  void do_cld(ClassLoaderData* cld) {
    if (_klass != NULL) {
      return;
    }

    if (_debug == NULL) {
      _debug = cld;
    }

    ResourceMark rm;
    tty->print_cr("CLDClosureTest::do_cld: %s", cld->loader_name());

    assert_locked_or_safepoint(SystemDictionary_lock);
    #ifndef ASSERT
    guarantee(VerifyBeforeGC      ||
              VerifyDuringGC      ||
              VerifyBeforeExit    ||
              VerifyDuringStartup ||
              VerifyAfterGC, "too expensive");
    #endif

    Dictionary* dictionary = cld->dictionary();
    if (dictionary == NULL) {
      tty->print_cr("CLDClosureTest::do_cld: dictionary is NULL");
      return;
    }

    unsigned int d_hash = dictionary->compute_hash(_sym);
    assert_locked_or_safepoint(SystemDictionary_lock);
    int index = dictionary->hash_to_index(d_hash);
    Klass* k = NULL;
    k = dictionary->find_class(index, d_hash, _sym);

    oop class_loader = cld->class_loader();
    if (class_loader == NULL) {
      tty->print_cr("CLDClosureTest::do_cld: class_loader is NULL");
      return;
    }
    k = SystemDictionary::resolve_or_null(_sym, Handle(_thread, cld->class_loader()), Handle(), _thread);

    Thread* __the_thread__ = _thread;
    if (HAS_PENDING_EXCEPTION) {
      if (PENDING_EXCEPTION->is_a(SystemDictionary::ClassNotFoundException_klass())) {
        CLEAR_PENDING_EXCEPTION;
      } else {
        assert(false, "");
      }
    }

    if (k != NULL) {
      _klass = k;
      tty->print_cr("CLDClosureTest::do_cld: find !!! %s", k->external_name());
    } else {
      if (_thread->is_active_Java_thread()) {
        //ClassLoader::load_class(s, false, thread);
      } else {
        tty->print_cr("CLDClosureTest::do_cld: not active Java thread");
      }
    }
  }
};

class CLDClosureTest2: public CLDClosure {
 public:
  Klass* _klass;
  Thread* _thread;
  Symbol* _sym;
  ClassLoaderData* _debug;

  CLDClosureTest2(Thread* thread, Symbol* sym): _klass(NULL), _thread(thread), _sym(sym), _debug(NULL) {}

  void do_cld(ClassLoaderData* cld) {
    if (_klass != NULL) {
      return;
    }

    if (_debug == NULL) {
      _debug = cld;
    }

    if (cld->is_boot_class_loader_data()) {
      //return;
    }

    Klass* k = NULL;

    ResourceMark rm;
    tty->print_cr("CLDClosureTest::do_cld: %s", cld->loader_name());

    oop class_loader = cld->class_loader();
    if (class_loader == NULL) {
      tty->print_cr("CLDClosureTest::do_cld: class_loader is NULL");
      return;
    }
    k = SystemDictionary::resolve_or_null(_sym, Handle(_thread, cld->class_loader()), Handle(), _thread);

    Thread* __the_thread__ = _thread;
    if (HAS_PENDING_EXCEPTION) {
      if (PENDING_EXCEPTION->is_a(SystemDictionary::ClassNotFoundException_klass())) {
        CLEAR_PENDING_EXCEPTION;
      } else {
        assert(false, "");
      }
    }

    if (k != NULL) {
      _klass = k;
      tty->print_cr("CLDClosureTest::do_cld: find !!! %s", k->external_name());
    } else {
      if (_thread->is_active_Java_thread()) {
        //ClassLoader::load_class(s, false, thread);
      } else {
        tty->print_cr("CLDClosureTest::do_cld: not active Java thread");
      }
    }
  }
};

class CLDClosureTest3: public CLDClosure {
 public:
  int* _cldsi;
  ClassLoaderData** _clds;

  CLDClosureTest3(int* cldsi, ClassLoaderData** clds): _cldsi(cldsi), _clds(clds) {}

  void do_cld(ClassLoaderData* cld) {
    int csi = *_cldsi;
    if (csi >= 100) {
      assert(false, "too many cld");
      return;
    }

    //tty->print_cr("CLDClosureTest::do_cld: %d", csi);
    _clds[csi] = cld;
    *_cldsi = csi + 1;
  }
};

class CLDClosureTest4: public CLDClosure {
 public:
  objArrayOop _obj_ary;
  int _len;

  CLDClosureTest4(objArrayOop obj_ary): _obj_ary(obj_ary), _len(0) {}

  void do_cld(ClassLoaderData* cld) {
    oop class_loader = cld->class_loader();
    if (class_loader != NULL) {
      tty->print_cr("CLDClosureTest::do_cld: %s", cld->loader_name());
      _obj_ary->obj_at_put(_len, class_loader);
      _len++;
    }
  }
};

class CLDObjectClosureTest : public ObjectClosure {
 private:
  objArrayOop _obj_ary;
  int _len;
 public:
  CLDObjectClosureTest(objArrayOop obj_ary): _obj_ary(obj_ary), _len(0) {}

  void do_object(oop obj) {
    if (obj->klass()->is_instance_klass()) {
      InstanceKlass* ik = InstanceKlass::cast(obj->klass());
      if (ik->is_class_loader_instance_klass()) {
        tty->print_cr("CLDObjectClosureTest::do_object: %s, alive: %d",
          ik->external_name(), obj->is_gc_marked());
        _obj_ary->obj_at_put(_len, obj);
        _len++;
      }
    }
  }
};

/*
#include "gc/shared/space.hpp"
class AdjustPointersClosure: public SpaceClosure {
 public:
  void do_space(Space* sp) {
    //sp->adjust_pointers();
    if (sp->used() == 0) {
      return;   // Nothing to do.
    }
  }
};

class GenAdjustPointersClosure: public GenCollectedHeap::GenClosure {
public:
  void do_generation(Generation* gen) {
    //gen->adjust_pointers();
    AdjustPointersClosure blk;
    gen->space_iterate(&blk);
  }
};
*/

int NVMRecovery::flag = 1;

#include "runtime/vmOperations.hpp"
#include "runtime/vmThread.hpp"
void NVMRecovery::test() {
  VM_OurPersistRecovery opr;
  VMThread::execute(&opr);

  NVMRecovery::main();
}

void NVMRecovery::main() {
  if (NVMRecovery::flag != 0) {
    NVMRecovery::flag++;
    return;
  }
  NVMRecovery::flag++;

  ResourceMark rm;

  tty->print_cr("NVMRecovery::test() step 1");
  Thread* thread = Thread::current();
  Thread* __the_thread__ = thread;
  //assert(thread->is_VM_thread(), "");
  Symbol* exception_noclass = vmSymbols::java_lang_NoClassDefFoundError();
  Symbol* exception_nullptr = vmSymbols::java_lang_NullPointerException();
  //const char name[] = "java/lang/Object";
  //const char name[] = "abc/Test";
  const char name[] = "org/apache/lucene/document/Field";
  //const char name[] = "A";
  //const char name[] = "java/lang/Shutdown";

  tty->print_cr("NVMRecovery::test() step 2");

  Symbol* s = SystemDictionary::class_name_symbol(name, exception_noclass, thread);
  if (s == NULL) {
    tty->print_cr("NVMRecovery::test() symbol is NULL");
    return;
  }

  Klass* k = NULL;

  tty->print_cr("NVMRecovery::test() step 3");
  if (k == NULL) {
    k = SystemDictionary::resolve_or_null(s, thread);
    if (HAS_PENDING_EXCEPTION) {
      if (PENDING_EXCEPTION->is_a(SystemDictionary::ClassNotFoundException_klass())) {
        CLEAR_PENDING_EXCEPTION;
      } else {
        assert(false, "");
      }
    }
    if (k != NULL) {
      tty->print_cr("NVMRecovery::test() find class %s", k->external_name());
    }
  }

  tty->print_cr("NVMRecovery::test() step 4");
  if (k == NULL) {
    k = SystemDictionary::resolve_or_null(s, Handle(thread, SystemDictionary::java_system_loader()),
                                          Handle(), thread);
    if (HAS_PENDING_EXCEPTION) {
      if (PENDING_EXCEPTION->is_a(SystemDictionary::ClassNotFoundException_klass())) {
        CLEAR_PENDING_EXCEPTION;
      } else {
        assert(false, "");
      }
    }
    if (k != NULL) {
      tty->print_cr("NVMRecovery::test() find class %s", k->external_name());
    }
  }

  tty->print_cr("NVMRecovery::test() step 5");
  if (k == NULL) {
    int cldsi = 0;
    ClassLoaderData* clds[100] = {NULL};
    {
      MutexLocker ml(ClassLoaderDataGraph_lock);
      CLDClosureTest3 cldct(&cldsi, clds);
      ClassLoaderDataGraph::cld_do(&cldct);
    }
    for (int i = cldsi-1; i>= 0; i--) {
      //tty->print_cr("NVMRecovery::test() clds[%d]", i);
      tty->print_cr("NVMRecovery::test() clds[%d] = %s", i, clds[i]->loader_name());

      oop class_loader = clds[i]->class_loader();
      if (class_loader == NULL) {
        //tty->print_cr("CLDClosureTest::do_cld: class_loader is NULL");
        continue;
      }
      k = SystemDictionary::resolve_or_null(s, Handle(thread, clds[i]->class_loader()), Handle(), thread);
      if (HAS_PENDING_EXCEPTION) {
        if (PENDING_EXCEPTION->is_a(SystemDictionary::ClassNotFoundException_klass())) {
          CLEAR_PENDING_EXCEPTION;
        } else {
          assert(false, "");
        }
      }

      if (k != NULL) {
        break;
      }
    }
    if (k != NULL) {
      tty->print_cr("NVMRecovery::test() find class %s", k->external_name());
    }
  }

  // test
  if (k != NULL) {
    InstanceKlass::cast(k)->initialize(CHECK);
    //oopDesc* obj = InstanceKlass::cast(k)->allocate_instance(thread);
    //tty->print_cr("NVMRecovery::test() %p", obj);
  } else {
    tty->print_cr("NVMRecovery::test() k == NULL");
  }
}

#include "nvm/recovery/nvmRecoveryClassLoaderCollector.hpp"
jint NVMRecovery::get_loader(jobject ary_obj, TRAPS) {
  VM_OurPersistRecoveryGetLoader opr(ary_obj);
  MutexLocker ml(Heap_lock);
  VMThread::execute(&opr);
  jint num = opr.num_loaders();
  if (num == -1 || num == -2) {
    // TODO: throw exception
    THROW_0(vmSymbols::java_lang_NullPointerException());
  }
  return num;
}

void NVMRecovery::collect_class_loader_during_gc() {
  NVMRecoveryClassLoaderCollector::collect_class_loader_during_gc();
}

void VM_OurPersistRecovery::doit() {
  assert(SafepointSynchronize::is_at_safepoint(), "");
  //NVMRecovery::main();
}

class OurPersistRecoveryCollectBootStrapCLD: public CLDClosure {
 private:
  int _cldsi;
  int _cldsi_max;
  ClassLoaderData** _clds;
  bool _error;
 public:
  OurPersistRecoveryCollectBootStrapCLD(int cldsi_max, ClassLoaderData** clds) {
    assert(cldsi_max > 0 && clds != NULL, "");
    _cldsi = 0;
    _cldsi_max = cldsi_max;
    _clds = clds;
    _error = false;
  }
  int cldsi() { return _cldsi; }
  bool error() { return _error; }
  void do_cld(ClassLoaderData* cld) {
    //tty->print_cr("CLDClosureTest000 name: %s, loader: %p", cld->loader_name(), cld);
    //if (cld->is_the_null_class_loader_data()) tty->print_cr("null loader.");
    //if (cld->is_boot_class_loader_data()) tty->print_cr("boot loader.");
    //if (cld->is_builtin_class_loader_data()) tty->print_cr("buildin loader.");
    if (cld->is_boot_class_loader_data()) {
      if (_cldsi == _cldsi_max) {
        _error = true;
        return;
      }
      _clds[_cldsi++] = cld;
    }
  }
};

class OurPersistInitKlassClosure : public KlassClosure {
 private:
  int _count;
 public:
  OurPersistInitKlassClosure() {
    _count = 0;
  };
  int count() { return _count; }

  void do_klass(Klass* k) {
    _count++;
    //tty->print_cr("do_klass %s", k->external_name());
  }
};

void VM_OurPersistRecoveryInit::doit() {
  OurPersistInitKlassClosure orp;
  //ClassLoaderData::the_null_class_loader_data()->classes_do(&orp);
  ClassLoaderDataGraph::classes_do(&orp);
  //Universe::basic_type_classes_do(&orp);




  OurPersistInitKlassClosure orp2;

  oop obj = JNIHandles::resolve(_ary_clds);
  if (obj == NULL) {
    //THROW_MSG(SymbolTable::new_symbol("ourpersist/RecoveryException"), "obj == NULL");
  }
  if (!obj->is_objArray()) {
    //THROW_MSG(SymbolTable::new_symbol("ourpersist/RecoveryException"), "!obj->is_objArray()");
  }
  objArrayOop obj_ary = objArrayOop(obj);
  int i = 0;
  for (; i < obj_ary->length(); i++) {
    oop elem = obj_ary->obj_at(i);
    if (elem == NULL) continue;

    Klass* k = elem->klass();
    if (!k->is_instance_klass()) {
      //THROW_MSG(SymbolTable::new_symbol("ourpersist/RecoveryException"), "Contains non-instance klass");
    }
    InstanceKlass* ik = InstanceKlass::cast(k);
    if (!ik->is_class_loader_instance_klass()) {
      //THROW_MSG(SymbolTable::new_symbol("ourpersist/RecoveryException"), "Contains non-class loader klass");
    }
    InstanceClassLoaderKlass* iclk = (InstanceClassLoaderKlass*)ik;

    ClassLoaderData* cld = java_lang_ClassLoader::loader_data_acquire(elem);
    while (elem != NULL && cld != NULL) {
      if (cld->is_boot_class_loader_data()) break;
      tty->print_cr("argument cld %s", cld->loader_name());
      cld->classes_do(&orp2);
      elem = java_lang_ClassLoader::parent(elem);
      if (elem != NULL) cld = java_lang_ClassLoader::loader_data_acquire(elem);
    }
  }
  // bootstrap class loader?
  //ClassLoaderData::the_null_class_loader_data()->classes_do(&orp2);

  ClassLoaderData* bootstrap_loaders[128];
  int bootstrap_loaders_size = sizeof(bootstrap_loaders) / sizeof(bootstrap_loaders[0]);
  OurPersistRecoveryCollectBootStrapCLD cldct(bootstrap_loaders_size, bootstrap_loaders);
  ClassLoaderDataGraph::cld_do(&cldct);
  assert(cldct.error() == false, "error");
  for (int i = 0; i < cldct.cldsi(); i++) {
    //tty->print_cr("bootstrap cld %s", bootstrap_loaders[i]->loader_name());
    bootstrap_loaders[i]->classes_do(&orp2);
  }


  tty->print_cr("[create_mirror] klass count: %d", NVMRecovery::create_mirror_count);
  tty->print_cr("[ClassLoaderDataGraph] klass count: %d", orp.count());
  tty->print_cr("[Argument] klass count: %d", orp2.count());

  assert(SafepointSynchronize::is_at_safepoint(), "");
  OurPersist::set_started();
}

#endif // OUR_PERSIST
