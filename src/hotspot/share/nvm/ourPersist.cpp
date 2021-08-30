#ifdef OUR_PERSIST

#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "nvm/nvmMacro.hpp"
#include "nvm/ourPersist.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klass.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/typeArrayKlass.hpp"

#include "classfile/javaClasses.hpp"
#include "runtime/thread.hpp"
#include "oops/instanceKlass.hpp"
#include "nvm/nvmAllocator.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "utilities/globalDefinitions.hpp"
#include "runtime/signature.hpp"
#include "nvm/nvmWorkListStack.hpp"
#include "nvm/nvmBarrierSync.inline.hpp"
#include "nvm/nvmMacro.hpp"
#include "runtime/fieldDescriptor.hpp"

bool OurPersist::_enable = false;
bool OurPersist::_enable_is_set = false;

void* OurPersist::allocate_nvm(int size, Thread* thr) {
  void* mem = NVMAllocator::allocate(size);
  assert(mem != NULL, "");

  if (thr == NULL) {
    thr = Thread::current();
  }
  assert(thr != NULL, "");

  OurPersist::set_responsible_thread(mem, thr);
  return mem;
}

bool OurPersist::is_volatile(oop obj, ptrdiff_t offset, DecoratorSet ds) {
  if (!(ds & OURPERSIST_BS_ASM)) return false; // DEBUG:
  if (ds & OURPERSIST_IS_VOLATILE) return true;
  if (ds & OURPERSIST_IS_NOT_VOLATILE) return false;
  ShouldNotReachHere(); // DEBUG:
  return OurPersist::is_volatile_slow(obj, offset, ds);
}

bool OurPersist::is_volatile_fast(oop obj, ptrdiff_t offset, DecoratorSet ds) {
  return (ds & OURPERSIST_IS_VOLATILE) || (ds & MO_SEQ_CST);
}

bool OurPersist::is_volatile_slow(oop obj, ptrdiff_t offset, DecoratorSet ds) {
  Klass* k = obj->klass();
  if (k->is_array_klass()) {
    return false;
  }
  assert(k->is_instance_klass(), "");

  int k_id = k->id();
  bool is_static = k_id == InstanceMirrorKlassID && InstanceMirrorKlass::offset_of_static_fields() <= offset;
  fieldDescriptor fd;
  ((InstanceKlass*)k)->find_field_from_offset(offset, is_static, &fd);
  bool is_volatile_FD = fd.is_volatile();

  return is_volatile_FD;
}

void OurPersist::copy_dram_to_nvm(oop from, oop to, ptrdiff_t offset, BasicType type, bool is_array) {
  const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP;
  typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
  typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;

  switch(type) {
    case T_BYTE:
      {
        jbyte byte_val = Parent::load_in_heap_at<jbyte>(from, offset);
        Raw::store_in_heap_at(to, offset, byte_val);
        break;
      }
    case T_CHAR:
      {
        jchar char_val = Parent::load_in_heap_at<jchar>(from, offset);
        Raw::store_in_heap_at(to, offset, char_val);
        break;
      }
    case T_DOUBLE:
      {
        jdouble double_val = Parent::load_in_heap_at<jdouble>(from, offset);
        Raw::store_in_heap_at(to, offset, double_val);
        break;
      }
    case T_FLOAT:
      {
        jfloat float_val = Parent::load_in_heap_at<jfloat>(from, offset);
        Raw::store_in_heap_at(to, offset, float_val);
        break;
      }
    case T_INT:
      {
        jint int_val = Parent::load_in_heap_at<jint>(from, offset);
        Raw::store_in_heap_at(to, offset, int_val);
        break;
      }
    case T_LONG:
      {
        jlong long_val = Parent::load_in_heap_at<jlong>(from, offset);
        Raw::store_in_heap_at(to, offset, long_val);
        break;
      }
    case T_SHORT:
      {
        jshort short_val = Parent::load_in_heap_at<jshort>(from, offset);
        Raw::store_in_heap_at(to, offset, short_val);
        break;
      }
    case T_BOOLEAN:
      {
        jboolean bool_val = Parent::load_in_heap_at<jboolean>(from, offset);
        Raw::store_in_heap_at(to, offset, bool_val);
        break;
      }
    case T_OBJECT:
    case T_ARRAY:
      {
        report_vm_error(__FILE__, __LINE__, "Should not reach here. Must execute ensure_recoverable for each oop value.");
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
}

bool OurPersist::cmp_dram_and_nvm(oop dram, oop nvm, ptrdiff_t offset, BasicType type, bool is_array) {
  const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP;
  typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
  typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;

  union {
    jbyte byte_val;
    jchar char_val;
    jdouble double_val;
    jfloat float_val;
    jint int_val;
    jlong long_val;
    jshort short_val;
    jboolean bool_val;
    oopDesc* oop_val;
  } v1, v2;
  v1.long_val = 0L;
  v2.long_val = 0L;

  switch(type) {
    case T_BYTE:
      {
        v1.byte_val = Parent::load_in_heap_at<jbyte>(dram, offset);
        v2.byte_val = Raw::load_in_heap_at<jbyte>(nvm, offset);
        break;
      }
    case T_CHAR:
      {
        v1.char_val = Parent::load_in_heap_at<jchar>(dram, offset);
        v2.char_val = Raw::load_in_heap_at<jchar>(nvm, offset);
        break;
      }
    case T_DOUBLE:
      {
        v1.double_val = Parent::load_in_heap_at<jdouble>(dram, offset);
        v2.double_val = Raw::load_in_heap_at<jdouble>(nvm, offset);
        break;
      }
    case T_FLOAT:
      {
        v1.float_val = Parent::load_in_heap_at<jfloat>(dram, offset);
        v2.float_val = Raw::load_in_heap_at<jfloat>(nvm, offset);
        break;
      }
    case T_INT:
      {
        v1.int_val = Parent::load_in_heap_at<jint>(dram, offset);
        v2.int_val = Raw::load_in_heap_at<jint>(nvm, offset);
        break;
      }
    case T_LONG:
      {
        v1.long_val = Parent::load_in_heap_at<jlong>(dram, offset);
        v2.long_val = Raw::load_in_heap_at<jlong>(nvm, offset);
        break;
      }
    case T_SHORT:
      {
        v1.short_val = Parent::load_in_heap_at<jshort>(dram, offset);
        v2.short_val = Raw::load_in_heap_at<jshort>(nvm, offset);
        break;
      }
    case T_BOOLEAN:
      {
        v1.bool_val = Parent::load_in_heap_at<jboolean>(dram, offset) & 0x1;
        v2.bool_val = Raw::load_in_heap_at<jboolean>(nvm, offset) & 0x1;
        break;
      }
    case T_OBJECT:
    case T_ARRAY:
      {
        oop dram_v = Parent::oop_load_in_heap_at(dram, offset);
        v1.oop_val = oop(dram_v != NULL ? dram_v->nvm_header().fwd() : NULL);
        v2.oop_val = Raw::oop_load_in_heap_at(nvm, offset);
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  return v1.long_val == v2.long_val;
}

Thread* OurPersist::responsible_thread(void* nvm_obj) {
  assert(!oopDesc::is_oop(oop(nvm_obj)), "nvm_obj should not be oop.");
  assert(nvm_obj != NULL, "Maybe nvm_obj is oop or initialization failed.");
  assert(nvm_obj != OURPERSIST_FWD_BUSY, "Maybe nvm_obj is oop or initialization failed.");

  return (Thread*)oop(nvm_obj)->nvm_header().fwd();
}

void OurPersist::set_responsible_thread(void* nvm_ptr, Thread* cur_thread) {
  oop(nvm_ptr)->set_mark(markWord::zero());

  if (cur_thread->dependent_obj_list_head() == NULL) {
    cur_thread->set_dependent_obj_list_head(nvm_ptr);
  } else {
    oop(cur_thread->dependent_obj_list_tail())->set_mark(markWord::from_pointer(nvm_ptr));
  }

  cur_thread->set_dependent_obj_list_tail(nvm_ptr);
  nvmHeader::set_fwd(oop(nvm_ptr), (void*)cur_thread);
}

void OurPersist::clear_responsible_thread(Thread* cur_thread) {
  void* dependent_obj = cur_thread->dependent_obj_list_head();

  while (dependent_obj != NULL) {
    oop cur_obj = oop(dependent_obj);
    nvmHeader::set_fwd(cur_obj, NULL);
    dependent_obj = cur_obj->mark().to_pointer();
  }

  cur_thread->set_dependent_obj_list_head(NULL);
  cur_thread->set_dependent_obj_list_tail(NULL);
}

bool OurPersist::check_durableroot_annotation(oop klass_obj, ptrdiff_t offset) {
  assert(klass_obj != NULL, "");
  assert(klass_obj->klass()->is_instance_klass(), "");
  assert(((InstanceKlass*)klass_obj->klass())->is_mirror_instance_klass(), "");

#ifdef ALL_DURABLEROOT_IS_TRUE
  return true;
#endif
#ifdef ALL_DURABLEROOT_IS_FALSE
  return false;
#endif

  // get field index
  Klass* k = java_lang_Class::as_Klass(klass_obj);
  assert(k != NULL, "");
  if (!k->is_instance_klass()) return false;
  assert(k->is_instance_klass(), "");
  InstanceKlass* ik = (InstanceKlass*)k;
  Array<AnnotationArray*>* fa = ik->fields_annotations();
  int index = -1;
  if (fa != NULL) {
    int cnt = ik->java_fields_count();
    for (int i = 0; i < cnt; i++) {
      if (ik->field_offset(i) == offset) {
        index = i;
        break;
      }
    }
  }
  if (index == -1) {
    k = klass_obj->klass();
    ik = (InstanceKlass*)k;
    fa = ik->fields_annotations();
    if (fa != NULL) {
      int cnt = ik->java_fields_count();
      for (int i = 0; i < cnt; i++) {
        if (ik->field_offset(i) == offset) {
          index = i;
          break;
        }
      }
    }
  }
  assert(fa == NULL || index != -1, "");

  // check annotation
  bool has_durableroot_annotation = false;
  if (fa != NULL) {
    AnnotationArray* anno = fa->at(index);
    if (anno != NULL) {
      int anno_len = anno->length();
      u2 num_annotations = anno->at(0) << 8 | anno->at(1);
      int i = 2;
      while (i < anno_len) {
        u2 type_index = anno->at(i) << 8 | anno->at(i+1);
        i += 2;
        assert(ik->constants()->tag_at(type_index).is_symbol(), "");
        Symbol* anno_sig = ik->constants()->symbol_at(type_index);
        if (anno_sig->equals("Ldurableroot;")) {
          has_durableroot_annotation = true;
          break;
        }
        // annotation
        u2 num_element_value_pairs = anno->at(i) << 8 | anno->at(i+1);
        i += 2;
        for (u2 j = 0; j < num_element_value_pairs; j++) {
          // element_value_pairs
          u2 element_name_index = anno->at(i) << 8 | anno->at(i+1);
          // element_value
          u1 element_value_tag = anno->at(i+2);
          i += 2 + 1;
          switch (element_value_tag) {
            case 'B': // byte
            case 'C': // char
            case 'D': // double
            case 'F': // float
            case 'I': // int
            case 'J': // long
            case 'S': // short
            case 'Z': // boolean
            case 's': // String
              // const_value_index
              // u2 const_value_index;
              i += 2;
              break;
            case 'e': // Enum type
              // enum_const_value
              // u2 type_name_index;
              // u2 const_name_index;
              i += 4;
              break;
            case 'c': // Class
              // class_info_index
              // u2 class_info_index;
              i += 2;
              break;
            case '@': // Annotation type
              // annotation_value
              // annotation annotation_value;
              assert(false, "TODO: Unimplemented.");
              break;
            case '[': // Array type
              // array_value
              // u2 num_values;
              // element_value values[num_values];
              assert(false, "TODO: Unimplemented.");
              break;
            default:
              assert(false, "illegal tag.");
          }
        }
      }
    }
  }
  return has_durableroot_annotation;
}

void OurPersist::ensure_recoverable(oop obj) {
  if (obj->nvm_header().recoverable()) {
    return;
  }

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();
  NVMBarrierSync* barrier_sync = cur_thread->nvm_barrier_sync();
  barrier_sync->init();

  void* nvm_obj = OurPersist::allocate_nvm(obj->size(), cur_thread);
RETRY:
  void* before_fwd = nvmHeader::cas_fwd(obj, NULL, nvm_obj);
  bool success = before_fwd == NULL;
  assert(!success || responsible_thread(nvm_obj) == Thread::current(), "");
  if (!success) {
    if (before_fwd == OURPERSIST_FWD_BUSY) {
      goto RETRY;
    }

    // TODO: release nvm

    Thread* dependant_thread = responsible_thread(nvm_obj);
    if (dependant_thread != NULL && dependant_thread != cur_thread) {
      barrier_sync->add(dependant_thread->nvm_barrier_sync(), obj);
    }
    barrier_sync->sync();

    OurPersist::clear_responsible_thread(cur_thread);
    return;
  }

  assert(responsible_thread(obj->nvm_header().fwd()) == Thread::current(), "");
  worklist->add(obj);

  while (worklist->empty() == false) {
    OurPersist::copy_object(worklist->remove());
  }

  // sfence
  NVM_FENCH

  barrier_sync->sync();

  OurPersist::clear_responsible_thread(cur_thread);
}

void OurPersist::copy_object(oop obj) {
  assert(responsible_thread(obj->nvm_header().fwd()) == Thread::current(), "");

  Klass* klass = obj->klass();
  void* nvm_obj = obj->nvm_header().fwd();
  size_t obj_size = obj->size() * HeapWordSize;

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();
  NVMBarrierSync* barrier_sync = cur_thread->nvm_barrier_sync();

RETRY:
  while (true) {
    // begin "for f in obj.fields"
    if (klass->is_instance_klass()) {
      Klass* cur_k = klass;
      while (true) {
        InstanceKlass* ik = (InstanceKlass*)cur_k;
        int cnt = ik->java_fields_count();

        for (int i = 0; i < cnt; i++) {
          AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
          Symbol* field_sig = ik->field_signature(i);
          field_sig->type();
          BasicType field_type = Signature::basic_type(field_sig);
          int field_offset = ik->field_offset(i);
          if (field_flags.is_static()) {
            continue;
          }

          if (is_reference_type(field_type)) {
            const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP;
            typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
            typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;
            oop v = Parent::oop_load_in_heap_at(obj, field_offset);
            void* nvm_v = NULL;

            if (v != NULL) {
              if (v->nvm_header().recoverable()) {
                nvm_v = v->nvm_header().fwd();
              } else {
                nvm_v = OurPersist::allocate_nvm(v->size(), cur_thread);
                while (true) {
                  void* before_fwd = nvmHeader::cas_fwd(v, NULL, nvm_v);
                  bool success = before_fwd == NULL;
                  assert(!success || responsible_thread(nvm_v) == Thread::current(), "");
                  if (success) {
                    assert(responsible_thread(obj->nvm_header().fwd()) == Thread::current(), "");
                    worklist->add(v);
                    break;
                  } else {
                    if (before_fwd == OURPERSIST_FWD_BUSY) {
                      // busy wait
                      continue;
                    }

                    // TODO: release nvm

                    nvm_v = v->nvm_header().fwd();

                    Thread* dependant_thread = OurPersist::responsible_thread(nvm_v);
                    if (dependant_thread != NULL && dependant_thread != cur_thread) {
                      barrier_sync->add(dependant_thread->nvm_barrier_sync(), v);
                    }

                    break;
                  }
                }
              }
            }
            Raw::oop_store_in_heap_at((oop)nvm_obj, field_offset, (oop)nvm_v);
          } else {
            OurPersist::copy_dram_to_nvm(obj, (oop)nvm_obj, field_offset, field_type);
          }
        }
        cur_k = cur_k->super();
        if (cur_k == NULL) {
          break;
        }
      }
    } else if (klass->is_objArray_klass()) {
      ObjArrayKlass* ak = (ObjArrayKlass*)klass;
      objArrayOop ao = (objArrayOop)obj;
      BasicType array_type = ((ArrayKlass*)ak)->element_type();
      int array_length = ao->length();

      for (int i = 0; i < array_length; i++) {
        ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
        const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY;
        typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
        typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;
        oop v = Parent::oop_load_in_heap_at(obj, field_offset);

        void* nvm_v = NULL;
        if (v != NULL) {
          if (v->nvm_header().recoverable()) {
            nvm_v = v->nvm_header().fwd();
          } else {
            nvm_v = OurPersist::allocate_nvm(v->size(), cur_thread);

            while (true) {
              void* before_fwd = nvmHeader::cas_fwd(v, NULL, nvm_v);
              bool success = before_fwd == NULL;
              assert(!success || responsible_thread(nvm_v) == Thread::current(), "");
              if (success) {
                assert(responsible_thread(v->nvm_header().fwd()) == Thread::current(), "");
                worklist->add(v);
                break;
              } else {
                if (before_fwd == OURPERSIST_FWD_BUSY) {
                  continue;
                }

                // TODO: release nvm

                nvm_v = v->nvm_header().fwd();

                Thread* dependant_thread = responsible_thread(nvm_v);
                if (dependant_thread != NULL && dependant_thread != cur_thread) {
                  barrier_sync->add(dependant_thread->nvm_barrier_sync(), v);
                }

                break;
              }
            }
          }
        }
        Raw::oop_store_in_heap_at((oop)nvm_obj, field_offset, (oop)nvm_v);
      }
    } else if (klass->is_typeArray_klass()) {
      TypeArrayKlass* ak = (TypeArrayKlass*)klass;
      typeArrayOop ao = (typeArrayOop)obj;
      BasicType array_type = ((ArrayKlass*)ak)->element_type();
      int array_length = ao->length();

      for (int i = 0; i < array_length; i++) {
        ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
        OurPersist::copy_dram_to_nvm(obj, (oop)nvm_obj, field_offset, array_type, true);
      }
    } else {
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
    }
    // end "for f in obj.fields"

    NVM_FLUSH_LOOP(nvm_obj, obj->size() * HeapWordSize);

    // verify step
    // begin "for f in obj.fields"
    if (klass->is_instance_klass()) {
      Klass* cur_k = klass;

      while(true) {
        InstanceKlass* ik = (InstanceKlass*)cur_k;
        int cnt = ik->java_fields_count();

        for (int i = 0; i < cnt; i++) {
          AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
          Symbol* field_sig = ik->field_signature(i);
          BasicType field_type = Signature::basic_type(field_sig);
          ptrdiff_t field_offset = ik->field_offset(i);

          if (field_flags.is_static()) {
            continue;
          }

          bool same = OurPersist::cmp_dram_and_nvm(obj, (oop)nvm_obj, field_offset, field_type, false);
          if (!same) {
            goto RETRY;
          }
        }

        cur_k = cur_k->super();
        if (cur_k == NULL) {
          break;
        }
      }
    } else if (klass->is_objArray_klass()) {
      ObjArrayKlass* ak = (ObjArrayKlass*)klass;
      objArrayOop ao = (objArrayOop)obj;
      BasicType array_type = ((ArrayKlass*)ak)->element_type();
      int array_length = ao->length();

      for (int i = 0; i < array_length; i++) {
        ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
        bool same = OurPersist::cmp_dram_and_nvm(obj, (oop)nvm_obj, field_offset, T_OBJECT, false);
        if (!same) {
          goto RETRY;
        }
      }
    } else if (klass->is_typeArray_klass()) {
      TypeArrayKlass* ak = (TypeArrayKlass*)klass;
      typeArrayOop ao = (typeArrayOop)obj;
      BasicType array_type = ((ArrayKlass*)ak)->element_type();
      int array_length = ao->length();

      for (int i = 0; i < array_length; i++) {
        ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
        bool same = OurPersist::cmp_dram_and_nvm(obj, (oop)nvm_obj, field_offset, array_type, true);
        if (!same) {
          goto RETRY;
        }
      }
    } else {
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
    }
    // end "for f in obj.fields"
    break;
  }
}

#endif // OUR_PERSIST
