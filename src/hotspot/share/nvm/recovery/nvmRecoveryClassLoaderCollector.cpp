#ifdef OUR_PERSIST

#include "nvm/recovery/nvmRecoveryClassLoaderCollector.hpp"
#include "runtime/jniHandles.inline.hpp"
#include "oops/access.inline.hpp"

class CLDObjectInGCClosureTest : public ObjectClosure {
 private:
  objArrayOop _obj_ary;
  jint _len;
  bool _skip_builtin_loader;
 public:
  CLDObjectInGCClosureTest(objArrayOop obj_ary, bool skip_builtin_loader):
    _obj_ary(obj_ary), _len(0), _skip_builtin_loader(skip_builtin_loader) {}

  jint len() { return _len; }
  void do_object(oop obj) {
    Klass* k = obj->klass();
    if (k->is_instance_klass()) {
      InstanceKlass* ik = InstanceKlass::cast(k);
      if (ik->is_class_loader_instance_klass()) {
        //tty->print_cr("alive: %d, addr: %p, nvm_fwd: %p, CLDObjectClosureTest::do_object: %s",
        //  obj->is_gc_marked(), OOP_TO_VOID(obj),
        //  obj->nvm_header().to_pointer(), ik->external_name());
        if (_obj_ary != NULL && obj->is_gc_marked()) {
          if (_skip_builtin_loader && ik->super() != NULL) {
            // skip builtin class loader
            const Symbol* builtin = vmSymbols::jdk_internal_loader_BuiltinClassLoader();
            Klass* cur = k;
            while (cur != NULL) {
              Symbol* sym = cur->name();
              if (sym == builtin) return;
              cur = cur->super();
            }
            //tty->print_cr("skip system class loader: %s, super: %s",
            //  ik->name()->as_C_string(), ik->super()->as_C_string());
            //if (s->equals("jdk/internal/loader/ClassLoaders$BootClassLoader"))          return;
            //if (s == vmSymbols::jdk_internal_loader_ClassLoaders_AppClassLoader())      return;
            //if (s == vmSymbols::jdk_internal_loader_ClassLoaders_PlatformClassLoader()) return;
          }
          _obj_ary->obj_field_put_raw(objArrayOopDesc::obj_at_offset<oop>(_len), obj);
          _len++;
        }
      }
    } else {
      //tty->print_cr("array: %s", obj->klass()->name()->as_C_string());
    }
  }
};

void VM_OurPersistRecoveryGetLoader::doit() {
  assert(SafepointSynchronize::is_at_safepoint(), "");
  _num_loaders = NVMRecoveryClassLoaderCollector::get_loader(_loader_ary_obj);
}

void NVMRecoveryClassLoaderCollector::collect_class_loader_during_gc() {
  assert(Universe::heap()->gc_cause() == GCCause::_ourpersist_collect_class_loader, "");
  tty->print_cr("[GC: ourpersist_collect_class_loader 0]");

  oop obj = JNIHandles::resolve(NVMRecoveryClassLoaderCollector::obj_ary_for_get_loader);
  assert(obj != NULL && obj->is_objArray(), "");
  objArrayOop obj_ary = objArrayOop(obj);

  bool skip_builtin_loader = NVMRecoveryClassLoaderCollector::skip_builtin_loader;
  CLDObjectInGCClosureTest cld_obj_closure(obj_ary, skip_builtin_loader);
  Universe::heap()->object_iterate(&cld_obj_closure);
  NVMRecoveryClassLoaderCollector::num_obj_ary_for_get_loader = cld_obj_closure.len();
  OrderAccess::fence();

  for (int i = 0; i < obj_ary->length(); i++) {
    oop obj = obj_ary->obj_at(i);
    if (obj != NULL) {
      InstanceKlass* ik = InstanceKlass::cast(obj->klass());
      if (ik->is_class_loader_instance_klass()) {
        tty->print_cr("CLDObjectClosureTest::do_object: %s, alive: %d, addr: %p",
          ik->external_name(), obj->is_gc_marked(), OOP_TO_VOID(obj));
      }
    }
  }
}


#include "gc/shared/genCollectedHeap.hpp"

jobject NVMRecoveryClassLoaderCollector::obj_ary_for_get_loader  = NULL;
jint NVMRecoveryClassLoaderCollector::num_obj_ary_for_get_loader = 0;
bool NVMRecoveryClassLoaderCollector::skip_builtin_loader = true;
jint NVMRecoveryClassLoaderCollector::get_loader(jobject jni_obj) {
  {
    jint res = 0;

    oop obj = JNIHandles::resolve(jni_obj);
    if (obj == NULL) {
      res = -1;
    }

    if (!obj->is_objArray()) {
      res = -1;
    }

    objArrayOop obj_ary = objArrayOop(obj);
    int len = obj_ary->length();
    if (len == 0) {
      res = -1;
    }

    if (NVMRecoveryClassLoaderCollector::obj_ary_for_get_loader != NULL) {
      res = -2;
    }

    if (NVMRecoveryClassLoaderCollector::num_obj_ary_for_get_loader != 0) {
      res = -2;
    }

    if (res != 0) {
      NVMRecoveryClassLoaderCollector::clear_obj_ary_for_get_loader();
      return res;
    }
  }

  NVMRecoveryClassLoaderCollector::obj_ary_for_get_loader = jni_obj;

  {
    //MutexLocker ml(ClassLoaderDataGraph_lock);

    //CLDClosureTest4 cld_closure(obj_ary);
    //tty->print_cr("NVMRecoveryClassLoaderCollector::get_loader: 1");
    //ClassLoaderDataGraph::cld_do(&cld_closure);
    //tty->print_cr("NVMRecoveryClassLoaderCollector::get_loader: 2");
    //ClassLoaderDataGraph::cld_unloading_do(&cld_closure);
  }

  {
    //Universe::heap()->collect(GCCause::_ourpersist_collect_class_loader);
    Universe::heap()->collect_as_vm_thread(GCCause::_ourpersist_collect_class_loader);
  }

  {
    Universe::heap()->ensure_parsability(false);
    if (false) {
      // Search the class loader data
      //CLDObjectClosureTest cld_obj_closure(obj_ary);
      //Universe::heap()->object_iterate(&cld_obj_closure);
    }
/*
    if (false) {
      GenCollectedHeap* gch = GenCollectedHeap::heap();
      GenAdjustPointersClosure blk;
      gch->generation_iterate(&blk, true);
    }
*/
  }

  jint res = NVMRecoveryClassLoaderCollector::num_obj_ary_for_get_loader;
  NVMRecoveryClassLoaderCollector::clear_obj_ary_for_get_loader();
  return res;
}

#endif // OUR_PERSIST
