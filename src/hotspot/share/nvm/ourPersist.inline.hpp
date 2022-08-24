#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_INLINE_HPP
#define NVM_OURPERSIST_INLINE_HPP

#include "nvm/ourPersist.hpp"
#include "nvm/nvmAllocator.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/fieldDescriptor.inline.hpp"
#include "utilities/globalDefinitions.hpp"

inline bool OurPersist::enable() {
  // init
  if (OurPersist::_enable == our_persist_unknown) {
    bool enable_slow = Arguments::is_interpreter_only() && !UseCompressedOops;
    OurPersist::_enable = enable_slow ? our_persist_true : our_persist_false;
    if (enable_slow) {
      tty->print("OurPersist is enabled.\n");
    } else {
      tty->print("OurPersist is disabled.\n");
    }
  }

  bool res = OurPersist::_enable == our_persist_true;
  assert(OurPersist::_enable != our_persist_unknown, "");
  return res;
}

inline bool OurPersist::started() {
  return OurPersist::_started == our_persist_true;
}

inline void OurPersist::set_started() {
  assert(Thread::current()->is_VM_thread(), "must be VM thread");
  OurPersist::_started = our_persist_true;
}

inline bool OurPersist::is_target(Klass* klass) {
  assert(klass != NULL, "");
  assert(OurPersist::is_target_slow(klass) == OurPersist::is_target_fast(klass), "");

  return OurPersist::is_target_fast(klass);
}

#ifdef ASSERT
inline bool OurPersist::is_target_slow(Klass* klass) {
  while (klass != NULL) {
    if (!OurPersist::is_target_fast(klass)) {
      return false;
    }
    klass = klass->super();
  }

  return true;
}
#endif // ASSERT

inline bool OurPersist::is_target_fast(Klass* klass) {
  int klass_id = klass->id();

  if (klass_id == InstanceMirrorKlassID) {
    return false;
  }
  //if (klass_id == InstanceRefKlassID) {
  //  return false;
  //}
  if (klass_id == InstanceClassLoaderKlassID) {
    return false;
  }

  return true;
}

inline bool OurPersist::is_static_field(oop obj, ptrdiff_t offset) {
  Klass* k = obj->klass();
  if (k->id() != InstanceMirrorKlassID) {
    return false;
  }
  if (offset < InstanceMirrorKlass::offset_of_static_fields()) {
    return false;
  }
  return true;
}

inline bool OurPersist::is_volatile(oop obj, ptrdiff_t offset, DecoratorSet ds) {
  if (ds & OURPERSIST_IS_NOT_VOLATILE) return false;
  if (ds & OURPERSIST_IS_VOLATILE)     return true;

  assert(oopDesc::is_oop(obj), "");

  Klass* k = obj->klass();
  if (k->is_array_klass()) return false;
  assert(k->is_instance_klass(), "");

  bool is_static = OurPersist::is_static_field(obj, offset);
  InstanceKlass* ik = InstanceKlass::cast(k);

  fieldDescriptor fd;
  ik->find_field_from_offset(offset, is_static /* is_static */, &fd);
  bool is_volatile = fd.is_volatile();

  return is_volatile;
}

// TODO: Use decorator set
inline bool OurPersist::is_durableroot(oop klass_obj, ptrdiff_t offset, DecoratorSet ds) {
  assert(klass_obj != NULL, "");
  assert(klass_obj->klass()->is_instance_klass(), "");
  assert(((InstanceKlass*)klass_obj->klass())->is_mirror_instance_klass(), "");

  Klass* k = java_lang_Class::as_Klass(klass_obj);
  assert(k != NULL, "");
  assert(k->is_instance_klass(), "");
  InstanceKlass* ik = InstanceKlass::cast(k);

  fieldDescriptor fd;
  bool success = ik->find_field_from_offset(offset, true /* is_static */, &fd);
  assert(success, "");
  bool is_durableroot = fd.is_durableroot();

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
  assert(is_durableroot, "must be durableroot");
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE
#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
  assert(!is_durableroot, "must not be durableroot");
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  return is_durableroot;
}

// NOTE: We assume the durableroot annotation and is-static flag is not changed
//       after the class is loaded, so the result of this function is immutable.
//       Therefore, calling this function before the fence is not a problem.
// NOTE: The same logic code exists in nvmCardTableBarrierSetAssembler_x86.cpp.
//       If you want to change the logic, you need to rewrite both codes.
inline bool OurPersist::needs_wupd(oop obj, ptrdiff_t offset, DecoratorSet ds, bool is_oop) {
  //assert(offset >= oopDesc::header_size() * HeapWordSize, "");
  // TODO: Using the decorator set.
  assert(obj != NULL, "");
  assert(oopDesc::is_oop(obj), "obj: %p", OOP_TO_VOID(obj));
  assert(obj->klass() != NULL, "");

  bool is_static = OurPersist::is_static_field(obj, offset);
  bool is_mirror = obj->klass()->id() == InstanceMirrorKlassID;

  // Primitive
  if (!is_oop) {
    return !is_mirror;
  }

  // Reference
  if (!is_mirror) {
    return true;
  }

  bool is_durable = is_static && OurPersist::is_durableroot(obj, offset, ds);
  if(is_durable) {
    return true;
  }

  return false;
}

inline void OurPersist::set_responsible_thread(void* nvm_obj, Thread* cur_thread) {
  assert(nvmHeader::is_fwd(nvm_obj), "");
  assert(cur_thread != NULL, "");

  oop(nvm_obj)->set_mark(markWord::zero());

  nvmHeader::set_fwd(oop(nvm_obj), (void*)cur_thread);
}

inline void OurPersist::clear_responsible_thread(Thread* cur_thread) {
  assert(cur_thread != NULL, "cur_thread: %p", cur_thread);

  void* dependent_obj = cur_thread->dependent_obj_list_head();

  while (dependent_obj != NULL) {
    oop cur_obj = oop(dependent_obj);
    nvmHeader::set_fwd(cur_obj, NULL);
    dependent_obj = cur_obj->mark().to_pointer();
  }

  cur_thread->set_dependent_obj_list_head(NULL);
  cur_thread->set_dependent_obj_list_tail(NULL);
}

inline void OurPersist::add_dependent_obj_list(void* nvm_obj, Thread* cur_thread) {
  if (cur_thread->dependent_obj_list_head() == NULL) {
    cur_thread->set_dependent_obj_list_head(nvm_obj);
  } else {
    oop(cur_thread->dependent_obj_list_tail())->set_mark(markWord::from_pointer(nvm_obj));
  }
  cur_thread->set_dependent_obj_list_tail(nvm_obj);
}

inline void* OurPersist::allocate_nvm(int word_size, Thread* thr) {
  NVM_COUNTER_ONLY(thr->nvm_counter()->inc_alloc_nvm(word_size);)

  void* mem = NVMAllocator::allocate(word_size);
  assert(mem != NULL, "");

  if (thr == NULL) {
    thr = Thread::current();
  }
  assert(thr != NULL, "");

  OurPersist::set_responsible_thread(mem, thr);
  return mem;
}

inline void OurPersist::copy_dram_to_nvm(oop from, oop to, ptrdiff_t offset, BasicType type, bool is_array) {
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

inline bool OurPersist::cmp_dram_and_nvm(oop dram, oop nvm, ptrdiff_t offset, BasicType type, bool is_array) {
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

        // NOTE: The same logic code exists in nvmDebug.cpp.
        //       If you want to change the logic, you need to rewrite both codes.
        bool skip = false;
        // Is the value not the target?
        skip = skip || dram_v != NULL && !OurPersist::is_target(dram_v->klass());
        // Is the field not the target?
        skip = skip || !OurPersist::needs_wupd(dram, offset, DECORATORS_NONE, true);
        if (skip) {
          assert(Raw::oop_load_in_heap_at(nvm, offset) == NULL, "should be NULL");
          return true;
        }

        v1.oop_val = oop(dram_v != NULL ? dram_v->nvm_header().fwd() : NULL);
        v2.oop_val = oop(Raw::oop_load_in_heap_at(nvm, offset));
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  return v1.long_val == v2.long_val;
}

inline Thread* OurPersist::responsible_thread(void* nvm_obj) {
  assert(nvmHeader::is_fwd(nvm_obj), "nvm_obj: %p", nvm_obj);

  return (Thread*)oop(nvm_obj)->nvm_header().fwd();
}

#endif // NVM_OURPERSIST_INLINE_HPP
#endif // OUR_PERSIST
