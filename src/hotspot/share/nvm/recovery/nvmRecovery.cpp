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
#include "nvm/ourPersist.inline.hpp"
#include "nvm/recovery/nvmRecovery.hpp"
#include "nvm/recovery/nvmRecoveryWorkListStack.hpp"
#include "oops/arrayOop.inline.hpp"
#include "oops/instanceKlass.inline.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/instanceOop.hpp"
#include "oops/klass.inline.hpp"
#include "oops/markWord.hpp"
#include "oops/method.hpp"
#include "oops/nvmHeader.inline.hpp"
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
bool NVMRecovery::_init_nvm = false;

void NVMRecovery::check_nvm_loaded(TRAPS) {
  if (!NVMRecovery::_init_nvm) {
    THROW_MSG(ourpersist_recovery_exception(), "NVM file has not been initialized");
  }
}

Klass* NVMRecovery::nvmCopy2klass(nvmOop nvm_copy, TRAPS) {
  nvmMirrorOop nvm_mirror = nvm_copy->klass();
  assert(nvm_mirror != NULL, "");

  return NVMRecovery::nvmMirrorCopy2klass(nvm_mirror, CHECK_NULL);
}

Klass* NVMRecovery::nvmMirrorCopy2klass(nvmMirrorOop nvm_mirror_copy, TRAPS) {
  oop dram_mirror = JNIHandles::resolve(nvm_mirror_copy->dram_mirror());
  assert(dram_mirror != NULL, "");
  assert(oopDesc::is_oop(dram_mirror), "");

  Klass* klass = java_lang_Class::as_Klass(dram_mirror);
  assert(klass != NULL, "");

  return klass;
}

void NVMRecovery::initNvmFile(JNIEnv* env, jclass clazz, jstring nvm_file_path, TRAPS) {
  if (NVMRecovery::_init_nvm) {
    THROW_MSG(ourpersist_recovery_exception(), "NVM file has been initialized");
  }

  const char* nvm_file_path_cstr =
    java_lang_String::as_utf8_string(JNIHandles::resolve(nvm_file_path));
  NVMAllocator::init(nvm_file_path_cstr);

  NVMRecovery::_init_nvm = true;
  NVMRecovery::check_nvm_loaded(CHECK);
  if (HAS_PENDING_EXCEPTION) return;
}

jboolean NVMRecovery::hasEnableNvmData(JNIEnv* env, jclass clazz, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK_0);
  if (HAS_PENDING_EXCEPTION) return JNI_FALSE;

  if (NvmMeta::meta()->_state_flag == 1) {
    return JNI_TRUE;
  }

  return JNI_FALSE;
}

void NVMRecovery::disableNvmData(JNIEnv* env, jclass clazz, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK);
  if (HAS_PENDING_EXCEPTION) return;

  NvmMeta::meta()->_state_flag = 0;
  NVM_WRITEBACK(NvmMeta::meta()->state_flag_addr());
}

void NVMRecovery::initInternal(JNIEnv* env, jclass clazz, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK);
  if (HAS_PENDING_EXCEPTION) return;

  VM_OurPersistRecoveryInit op(env, clazz);
  VMThread::execute(&op);
  if (op.result() == JNI_FALSE) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "Error in NVMRecovery::initInternal");
  }
}

jobjectArray NVMRecovery::nvmCopyClassNames(JNIEnv* env, jclass clazz, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK_NULL);
  if (HAS_PENDING_EXCEPTION) return NULL;

  nvmMirrorOopDesc* head = NvmMeta::meta()->_mirrors_head;
  nvmMirrorOopDesc* cur = head;
  int count = 0;
  while (cur != NULL) {
    count++;
    cur = cur->class_list_next();
  }

  Klass* ak = Universe::objectArrayKlassObj();
  objArrayOop alloc_ary = ObjArrayKlass::cast(ak)->allocate(count, CHECK_NULL);
  objArrayHandle arr(THREAD, alloc_ary);

  cur = head;
  for (int i = 0; i < count; i++) {
    const char* str = cur->klass_name();
    Handle name = java_lang_String::create_from_str(str, THREAD);
    arr()->obj_at_put(i, name());
    cur = cur->class_list_next();
  }

  jobjectArray res = jobjectArray(JNIHandles::make_local(arr()));
  return res;
}

/*
oop resolve_class_oop(objArrayOop classes, const char* name) {
  int len = classes->length();
  ResourceMark rm; // DEBUG:
  int buf_len = strlen(name) + 10;
  char* buf = NEW_RESOURCE_ARRAY(char, buf_len);
  for (int i = 0; i < len; i++) {
    oop obj = classes->obj_at(i);
    Klass* k = java_lang_Class::as_Klass(obj);
    const char* str = k->name()->as_C_string(buf, buf_len);
    if (strcmp(str, name) == 0) {
      return obj;
    }
  }

  // DEBUG:
  //Symbol* sym = SymbolTable::new_symbol(name);
  // FIXME: thread --> CHECK_NULL
  //Klass* k = SystemDictionary::resolve_or_null(sym, Thread::current());
  //if (k == NULL) {
  //  return NULL;
  //}
  //return k->java_mirror();

  return NULL;
}

jclass resolve_class(jobjectArray classes, const char* name) {
  objArrayOop ary = objArrayOop(JNIHandles::resolve(classes));
  oop res = resolve_class_oop(ary, name);
  if (res == NULL) {
    return NULL;
  }
  return (jclass)JNIHandles::make_local(res);;
}
*/

void NVMRecovery::createDramCopy(JNIEnv* env, jclass clazz, jobjectArray dram_copy_list,
                                 jobjectArray classes, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK);
  if (HAS_PENDING_EXCEPTION) return;

  nvmMirrorOopDesc* mirrors_head = NvmMeta::meta()->_mirrors_head;

  // Set pointer to dram copy in nvmMirrorOop
  {
    nvmMirrorOopDesc* nvm_mirror = mirrors_head;
    int offset = 0;
    while (nvm_mirror != NULL) {
      oop dram_mirror = objArrayOop(JNIHandles::resolve(classes))->obj_at(offset);
      offset++;

#ifdef ASSERT
      ResourceMark rm;
      assert(dram_mirror != NULL, "");
      assert(nvm_mirror != NULL, "");
      assert(java_lang_Class::as_Klass(dram_mirror)->name()->equals(nvm_mirror->klass_name()), "");
#endif // ASSERT

      nvm_mirror->set_dram_mirror(JNIHandles::make_global(Handle(THREAD, dram_mirror)));
      nvm_mirror = nvm_mirror->class_list_next();
    }
  }

  // FIXME: delete cheap
  NVMRecoveryWorkListStack* worklist = new NVMRecoveryWorkListStack();

  // Collect all nvm mirrors
  {
    nvmMirrorOopDesc* cur = mirrors_head;
    while (cur != NULL) {
      Klass* klass = NVMRecovery::nvmMirrorCopy2klass(cur, CHECK);

      if (klass->is_instance_klass()) {
        Klass* cur_k = klass;
        while (cur_k != NULL) {
          InstanceKlass* ik = InstanceKlass::cast(cur_k);
          int cnt = ik->java_fields_count();

          for (int i = 0; i < cnt; i++) {
            AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
            Symbol* field_sig = ik->field_signature(i);
            field_sig->type();
            BasicType field_type = Signature::basic_type(field_sig);
            int field_offset = ik->field_offset(i);
            if (!field_flags.is_durableroot()) {
              continue;
            }
            assert(is_reference_type(field_type), "field is not reference");
            assert(field_flags.is_static(), "field is not static");

            nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(cur, field_offset);
            if (nvm_v == NULL) {
              continue;
            }

            if (!nvm_v->mark()) {
              nvm_v->set_mark();
              worklist->add(nvm_v);
            }
          }
          // DEBUG:
          //cur_k = cur_k->super();
          cur_k = NULL;
        }
      }

      cur = cur->class_list_next();
    }
  }

  // Get dram_copy_list length
  int list_length;
  {
    oop dram_copy_list_oop = JNIHandles::resolve(dram_copy_list);
    if (dram_copy_list_oop == NULL) {
      THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "dram_copy_list is null");
    }
    list_length = objArrayOop(dram_copy_list_oop)->length();
  }
  int list_count = 0;

  // Collect reachable objects
  while (worklist->empty() == false) {
    nvmOop obj = worklist->remove();
    Klass* klass = NVMRecovery::nvmCopy2klass(obj, CHECK);

    // Create a new object in DRAM and add it to dram_copy_list
    {
      // Check empty slot
      if (list_count == list_length - 1) {
        Klass* k = Universe::objectArrayKlassObj();
        assert(k != NULL && k->is_objArray_klass(), "sanity check");
        ObjArrayKlass* oak = ObjArrayKlass::cast(k);
        objArrayOop next = oak->allocate(list_length, THREAD);
        if (next == NULL) {
          THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "Out of memory");
        }

        objArrayOop prev = objArrayOop(JNIHandles::resolve(dram_copy_list));
        prev->obj_at_put(list_count, next);
        JNIHandles::destroy_local(dram_copy_list);
        dram_copy_list = jobjectArray(JNIHandles::make_local(next));
        list_count = 0;
      }

      // Allocate dram copy
      oop dram_copy;
      if (klass->is_instance_klass()) {
        InstanceKlass* ik = InstanceKlass::cast(klass);
        dram_copy = ik->allocate_instance(THREAD);
      } else if (klass->is_objArray_klass()) {
        ObjArrayKlass* oak = ObjArrayKlass::cast(klass);
        int array_length = objArrayOop(obj)->length();
        objArrayOop new_obj = oak->allocate(array_length, THREAD);
        new_obj->set_length(array_length);
        dram_copy = new_obj;
      } else if (klass->is_typeArray_klass()) {
        TypeArrayKlass* tak = TypeArrayKlass::cast(klass);
        int array_length = typeArrayOop(obj)->length();
        typeArrayOop new_obj = tak->allocate(array_length, THREAD);
        new_obj->set_length(array_length);
        dram_copy = new_obj;
      } else {
        THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "Unknown klass type");
      }
      if (dram_copy == NULL) {
        THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "Out of memory");
      }

      // Set forwardinng pointer to nvm copy
      nvmHeader::set_fwd(dram_copy, obj);

      // Add to dram_copy_list
      oop obj_oop = JNIHandles::resolve(dram_copy_list);
      assert(obj_oop != NULL, "dram_copy_list is null");
      objArrayOop obj_ary = objArrayOop(obj_oop);
      obj_ary->obj_at_put(list_count, dram_copy);
      list_count++;
    }

    // begin "for f in obj.fields"
    if (klass->is_instance_klass()) {
      Klass* cur_k = klass;
      while (cur_k != NULL) {
        InstanceKlass* ik = (InstanceKlass*)cur_k;
        int cnt = ik->java_fields_count();

        for (int i = 0; i < cnt; i++) {
          AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
          Symbol* field_sig = ik->field_signature(i);
          BasicType field_type = Signature::basic_type(field_sig);
          int field_offset = ik->field_offset(i);

          if (field_flags.is_static()) {
            continue;
          }
          if (is_java_primitive(field_type)) {
            continue;
          }

          nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(obj, field_offset);
          if (nvm_v == NULL) {
            continue;
          }
          if (!nvm_v->mark()) {
            nvm_v->set_mark();
            worklist->add(nvm_v);
          }
        }
        cur_k = cur_k->super();
      }
    } else if (klass->is_objArray_klass()) {
      ObjArrayKlass* ak = (ObjArrayKlass*)klass;
      BasicType array_type = ((ArrayKlass*)ak)->element_type();

      objArrayOop ao = (objArrayOop)obj;
      int array_length = ao->length();

      for (int i = 0; i < array_length; i++) {
        ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;

        nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(obj, field_offset);
        if (nvm_v == NULL) {
          continue;
        }
        if (!nvm_v->mark()) {
          nvm_v->set_mark();
          worklist->add(nvm_v);
        }
      }
    } else if (klass->is_typeArray_klass()) {
      // nothing to do
    } else {
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
    }
    // end "for f in obj.fields"
  }
}

void NVMRecovery::recoveryDramCopy(JNIEnv* env, jclass clazz, jobjectArray dram_copy_list,
                                   jobjectArray classes, TRAPS) {
  NVMRecovery::check_nvm_loaded(CHECK);
  if (HAS_PENDING_EXCEPTION) return;

  VM_OurPersistRecoveryDramCopy op(env, clazz, dram_copy_list, classes);
  VMThread::execute(&op);

  if (op.result() == JNI_FALSE) {
    THROW_MSG(NVMRecovery::ourpersist_recovery_exception(), "NVMRecovery::recoveryDramCopy failed");
  }
}

void NVMRecovery::killMe(JNIEnv *env, jclass clazz, TRAPS) {
  os::signal_raise(SIGKILL);
}

jstring NVMRecovery::mode(JNIEnv* env, jclass clazz, TRAPS) {
  const char* mode = "DEFAULT";
#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
  mode = "OURPERSIST_DURABLEROOTS_ALL_TRUE";
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE
#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
  mode = "OURPERSIST_DURABLEROOTS_ALL_FALSE";
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  Handle str = java_lang_String::create_from_str(mode, CHECK_NULL);
  return jstring(JNIHandles::make_local(str()));
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

    if (!OurPersist::is_target_mirror(k)) {
      return;
    }

    oop mirror = k->java_mirror();
    assert(mirror != NULL, "");

    nvmMirrorOop nvm_mirror = nvmMirrorOop(mirror->nvm_header().fwd());
    if (nvm_mirror == NULL) {
      //tty->print_cr("nvm_mirror is null: %s", k->name()->as_C_string()); // DEBUG:
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
  NvmMeta::meta()->_nvm_head = (char*)0x700000000000 + sizeof(NvmMeta);
  NVMAllocator::nvm_head = (char*)0x700000000000 + sizeof(NvmMeta);
  NvmMeta::meta()->_mirrors_head = NULL;
  nvmMirrorOopDesc::class_list_tail = NULL;
  NVM_WRITEBACK(NvmMeta::meta()->nvm_head_addr());
  NVM_WRITEBACK(NvmMeta::meta()->mirrors_head_addr());

  OurPersistSetNvmMirrors klass_closure(0, NULL);
  ClassLoaderDataGraph::classes_do(&klass_closure);
  if (klass_closure.result() == JNI_FALSE) return;

  NvmMeta::meta()->_state_flag = 1;
  NVM_WRITEBACK(NvmMeta::meta()->state_flag_addr());
  OurPersist::set_started();

  _result = JNI_TRUE;
}

void VM_OurPersistRecoveryDramCopy::doit() {
  objArrayOop dram_copy_list_oop = objArrayOop(JNIHandles::resolve(_dram_copy_list));
  if (dram_copy_list_oop == NULL) return;
  int list_length = dram_copy_list_oop->length();
  objArrayOop classes_oop = objArrayOop(JNIHandles::resolve(_classes));
  if (classes_oop == NULL) return;

  // Set forwarding pointer to dram copy
  {
    objArrayOop cur = dram_copy_list_oop;
    while (cur != NULL) {
      for (int i = 0; i < list_length - 1; i++) {
        oop obj = cur->obj_at(i);
        if (obj == NULL) continue;

        nvmOop nvm_obj = obj->nvm_header().fwd();
        assert(nvm_obj != NULL, "");

        nvm_obj->set_dram_copy(obj);
      }
      cur = objArrayOop(cur->obj_at(list_length - 1));
    }
  }
  {
    nvmMirrorOopDesc* head = NvmMeta::meta()->_mirrors_head;
    nvmMirrorOopDesc* cur = head;
    while (cur != NULL) {
      Klass* klass = NVMRecovery::nvmMirrorCopy2klass(cur, Thread::current());

      cur->set_mark();
      cur->set_dram_copy(klass->java_mirror());
      cur = cur->class_list_next();
    }
  }

  // Recovery
  {
    objArrayOop cur = dram_copy_list_oop;
    while (cur != NULL) {
      for (int i = 0; i < list_length - 1; i++) {
        oop obj = cur->obj_at(i);
        if (obj == NULL) continue;

        nvmOop nvm_obj = obj->nvm_header().fwd();
        assert(nvm_obj != NULL, "");

        Klass* klass = NVMRecovery::nvmCopy2klass(nvm_obj, Thread::current());

        // begin "for f in obj.fields"
        if (klass->is_instance_klass()) {
#ifdef OURPERSIST_INSTANCE_MAYBE_FASTCOPY
          void* obj_ptr = OOP_TO_VOID(obj);
          int base_offset_in_bytes = instanceOopDesc::base_offset_in_bytes();
          int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
          assert(base_offset_in_bytes % HeapWordSize == 0, "");
          HeapWord* from = (HeapWord*)((char*)nvm_obj + base_offset_in_bytes);
          HeapWord* to   = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
          Copy::aligned_disjoint_words(from, to, obj->size() - base_offset_in_words /* word size */);
#endif // OURPERSIST_INSTANCE_MAYBE_FASTCOPY
          Klass* cur_k = klass;
          while (cur_k != NULL) {
            InstanceKlass* ik = (InstanceKlass*)cur_k;
            int cnt = ik->java_fields_count();

            for (int i = 0; i < cnt; i++) {
              AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
              Symbol* field_sig = ik->field_signature(i);
              BasicType field_type = Signature::basic_type(field_sig);
              int field_offset = ik->field_offset(i);

              if (field_flags.is_static()) {
                continue;
              }
              if (is_java_primitive(field_type)) {
#ifndef OURPERSIST_INSTANCE_MAYBE_FASTCOPY
                OurPersist::copy_nvm_to_dram(nvm_obj, obj, field_offset, field_type);
#endif // OURPERSIST_INSTANCE_MAYBE_FASTCOPY
              } else {
                nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(nvm_obj, field_offset);
                oop v = nvm_v != NULL ? nvm_v->dram_copy() : NULL;
                RawAccess<>::oop_store_at(obj, field_offset, v);
              }
            }
            cur_k = cur_k->super();
          }
        } else if (klass->is_objArray_klass()) {
          ObjArrayKlass* ak = (ObjArrayKlass*)klass;
          BasicType array_type = ((ArrayKlass*)ak)->element_type();

          objArrayOop ao = (objArrayOop)obj;
          int array_length = ao->length();

          for (int i = 0; i < array_length; i++) {
            ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
            nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(nvm_obj, field_offset);
            oop v = nvm_v != NULL ? nvm_v->dram_copy() : NULL;
            RawAccess<>::oop_store_at(ao, field_offset, v);
          }
        } else if (klass->is_typeArray_klass()) {
          TypeArrayKlass* ak = (TypeArrayKlass*)klass;
          BasicType array_type = ((ArrayKlass*)ak)->element_type();
#ifdef OURPERSIST_TYPEARRAY_SLOWCOPY
          typeArrayOop ao = (typeArrayOop)obj;
          int array_length = ao->length();
          for (int i = 0; i < array_length; i++) {
            ptrdiff_t field_offset = typeArrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
            OurPersist::copy_nvm_to_dram(nvm_obj, ao, field_offset, array_type);
          }
#else // OURPERSIST_TYPEARRAY_SLOWCOPY
          void* obj_ptr = OOP_TO_VOID(obj);
          int base_offset_in_bytes = typeArrayOopDesc::base_offset_in_bytes(array_type);
          int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
          assert(base_offset_in_bytes % HeapWordSize == 0, "");
          HeapWord* from = (HeapWord*)((char*)nvm_obj + base_offset_in_bytes);
          HeapWord* to   = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
          Copy::aligned_disjoint_words(from, to, obj->size() - base_offset_in_words /* word size */);
#endif // OURPERSIST_TYPEARRAY_SLOWCOPY
        } else {
          report_vm_error(__FILE__, __LINE__, "Illegal field type.");
        }
        // end "for f in obj.fields"
      }
      cur = objArrayOop(cur->obj_at(list_length - 1));
    }
  }

  // Set all durableroots and init allocator
  nvmMirrorOopDesc* head = NvmMeta::meta()->_mirrors_head;
  nvmMirrorOopDesc* cur = head;
  while (cur != NULL) {
    nvmMirrorOopDesc::class_list_tail = cur;
    Klass* klass = NVMRecovery::nvmMirrorCopy2klass(cur, Thread::current());
    nvmHeader::set_fwd(klass->java_mirror(), cur);

    if (klass->is_instance_klass()) {
      Klass* cur_k = klass;
      while (cur_k != NULL) {
        InstanceKlass* ik = InstanceKlass::cast(cur_k);
        int cnt = ik->java_fields_count();

        for (int i = 0; i < cnt; i++) {
          AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
          Symbol* field_sig = ik->field_signature(i);
          field_sig->type();
          BasicType field_type = Signature::basic_type(field_sig);
          int field_offset = ik->field_offset(i);
          if (!field_flags.is_durableroot()) {
            continue;
          }
          assert(is_reference_type(field_type), "field is not reference");
          assert(field_flags.is_static(), "field is not static");

          nvmOop nvm_v = nvmOopDesc::load_at<nvmOop>(cur, field_offset);
          oop v = nvm_v != NULL ? nvm_v->dram_copy() : NULL;
          RawAccess<>::oop_store_at(cur->dram_copy(), field_offset, v);
        }
        // DEBUG:
        //cur_k = cur_k->super();
        cur_k = NULL;
      }
    }

    cur = cur->class_list_next();
  }

#ifdef ASSERT
  // Verify
  {
    objArrayOop cur = dram_copy_list_oop;
    while (cur != NULL) {
      for (int i = 0; i < list_length - 1; i++) {
        oop obj = cur->obj_at(i);
        if (obj == NULL) continue;
        // TODO: verify
        if (obj != NULL) {
          assert(oopDesc::is_oop(obj), "");
          assert(obj->nvm_header().fwd() != NULL, "");
        }
      }
      cur = objArrayOop(cur->obj_at(list_length - 1));
    }
  }
#endif // ASSERT

  {
    objArrayOop cur = dram_copy_list_oop;
    while (cur != NULL) {
      for (int i = 0; i < list_length - 1; i++) {
        oop obj = cur->obj_at(i);
        if (obj == NULL) continue;

        nvmOop nvm_obj = obj->nvm_header().fwd();
        assert(nvm_obj != NULL, "");

        nvm_obj->clear_dram_copy_and_mark();
      }
      cur = objArrayOop(cur->obj_at(list_length - 1));
    }
  }
  {
    nvmMirrorOopDesc* head = NvmMeta::meta()->_mirrors_head;
    nvmMirrorOopDesc* cur = head;
    while (cur != NULL) {
      cur->clear_dram_copy_and_mark();
      cur = cur->class_list_next();
    }
  }

  // Set fowarding pointer to nvm copy in mirrors
  // FIXME: set arguments
  OurPersistSetNvmMirrors klass_closure(0, NULL);
  ClassLoaderDataGraph::classes_do(&klass_closure);
  if (klass_closure.result() == JNI_FALSE) return;

  NvmMeta::meta()->_state_flag = 1;
  NVM_WRITEBACK(NvmMeta::meta()->state_flag_addr());
  OurPersist::set_started();

  _result = JNI_TRUE;
}

#endif // OUR_PERSIST
