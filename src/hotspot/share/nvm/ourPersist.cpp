#ifdef OUR_PERSIST

#include "classfile/javaClasses.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "nvm/nvmBarrierSync.inline.hpp"
#include "nvm/nvmMacro.hpp"
#include "nvm/nvmWorkListStack.hpp"
#include "nvm/ourPersist.inline.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klass.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/fieldDescriptor.inline.hpp"
#include "runtime/signature.hpp"
#include "runtime/thread.hpp"
#include "utilities/globalDefinitions.hpp"

int OurPersist::_enable = our_persist_not_set;

Thread* OurPersist::responsible_thread_noinline(void* nvm_obj) {
  return OurPersist::responsible_thread(nvm_obj);
}

void OurPersist::ensure_recoverable(oop obj) {
#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
  ShouldNotReachHere();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

#ifdef NO_ENSURE_RECOVERABLE
  return;
#endif // NO_ENSURE_RECOVERABLE

  if (obj->nvm_header().recoverable()) {
    return;
  }
  if (!OurPersist::is_target(obj->klass())) {
    return;
  }

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();
  NVMBarrierSync* barrier_sync = cur_thread->nvm_barrier_sync();
  barrier_sync->init();

  bool success = OurPersist::shade(obj, cur_thread);
  void* nvm_obj = obj->nvm_header().fwd();
  if (!success) {
    barrier_sync->add(obj, nvm_obj, cur_thread);
    barrier_sync->sync();
    return;
  }

  assert(nvm_obj == obj->nvm_header().fwd(),
         "nvm_obj: %p, fwd: %p", nvm_obj, obj->nvm_header().fwd());
  assert(responsible_thread(obj->nvm_header().fwd()) == Thread::current(),
         "fwd: %p", obj->nvm_header().fwd());
  worklist->add(obj);

  while (worklist->empty() == false) {
    NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_persistent_obj();)

    oop cur_obj = worklist->remove();
    void* cur_nvm_obj = cur_obj->nvm_header().fwd();

    OurPersist::add_dependent_obj_list(cur_nvm_obj, cur_thread);
    OurPersist::copy_object(cur_obj);
  }

  // sfence
  NVM_FENCH

  barrier_sync->sync();
  OurPersist::clear_responsible_thread(cur_thread);
}

void OurPersist::copy_object_copy_step(oop obj, void* nvm_obj, Klass* klass,
                                       NVMWorkListStack* worklist, NVMBarrierSync* barrier_sync,
                                       Thread* cur_thread) {
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

          if (v != NULL && OurPersist::is_target(v->klass())) {
            if (v->nvm_header().recoverable()) {
              nvm_v = v->nvm_header().fwd();
            } else {
              bool success = OurPersist::shade(v, cur_thread);
              nvm_v = v->nvm_header().fwd();
              assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
              if (success) {
                assert(responsible_thread(v->nvm_header().fwd()) == Thread::current(),
                       "fwd: %p", v->nvm_header().fwd());

                worklist->add(v);
              } else {
                barrier_sync->add(v, nvm_v, cur_thread);
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
      if (v != NULL && OurPersist::is_target(v->klass())) {
        if (v->nvm_header().recoverable()) {
          nvm_v = v->nvm_header().fwd();
        } else {
          bool success = OurPersist::shade(v, cur_thread);
          nvm_v = v->nvm_header().fwd();
          assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
          if (success) {
            assert(responsible_thread(v->nvm_header().fwd()) == Thread::current(),
                   "fwd: %p", v->nvm_header().fwd());

            worklist->add(v);
          } else {
            barrier_sync->add(v, nvm_v, cur_thread);
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
}

bool OurPersist::copy_object_verify_step(oop obj, void* nvm_obj, Klass* klass) {
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
          return false;
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
        return false;
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
        return false;
      }
    }
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  // end "for f in obj.fields"
  return true;
}

void OurPersist::copy_object(oop obj) {
  assert(obj != NULL, "obj: %p", OOP_TO_VOID(obj));
  assert(obj->nvm_header().fwd() != NULL, "fwd: %p", obj->nvm_header().fwd());
  assert(responsible_thread(obj->nvm_header().fwd()) == Thread::current(),
         "fwd: %p", obj->nvm_header().fwd());

  Klass* klass = obj->klass();
  void* nvm_obj = obj->nvm_header().fwd();
  size_t obj_size = obj->size() * HeapWordSize;

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();
  NVMBarrierSync* barrier_sync = cur_thread->nvm_barrier_sync();

  nvmHeader::lock(obj);

  while (true) {
    // copy step
    OurPersist::copy_object_copy_step(obj, nvm_obj, klass, worklist, barrier_sync, cur_thread);

    // mfence
    OrderAccess::fence();

    // write back & sfence
    NVM_FLUSH_LOOP(nvm_obj, obj->size() * HeapWordSize);

    // verify step
    bool success = OurPersist::copy_object_verify_step(obj, nvm_obj, klass);
    if (success) {
      break;
    }

    // retry
    NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_copy_obj_retry();)
  }

  nvmHeader::unlock(obj);
}

#endif // OUR_PERSIST
