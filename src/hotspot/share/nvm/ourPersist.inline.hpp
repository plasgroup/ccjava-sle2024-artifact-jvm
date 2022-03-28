#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_INLINE_HPP
#define NVM_OURPERSIST_INLINE_HPP

#include "nvm/ourPersist.hpp"
#include "nvm/nvmAllocator.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/fieldDescriptor.hpp"

inline bool OurPersist::enable() {
  // init
  if (OurPersist::_enable == our_persist_not_set) {
    bool enable_slow = Arguments::is_interpreter_only() && !UseCompressedOops;
    OurPersist::_enable = enable_slow ? our_persist_enable : our_persist_disable;
    if (OurPersist::_enable) {
      tty->print("OurPersist is enabled.\n");
    } else {
      tty->print("OurPersist is disabled.\n");
    }
  }

  bool res = OurPersist::_enable == our_persist_enable;
  assert(OurPersist::_enable != our_persist_not_set, "");
  return res;
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

inline bool OurPersist::is_volatile_and_non_mirror(oop obj, ptrdiff_t offset, DecoratorSet ds) {
  if (ds & OURPERSIST_IS_NOT_VOLATILE) return false;
  if (ds & OURPERSIST_IS_VOLATILE)     return true;
  if (ds & OURPERSIST_IS_STATIC)       return false;

  assert(oopDesc::is_oop(obj), "");

  Klass* k = obj->klass();
  if (k->is_array_klass()) return false;
  assert(k->is_instance_klass(), "");

  InstanceKlass* ik = (InstanceKlass*)k;
  if (ik->is_mirror_instance_klass()) return false;

  fieldDescriptor fd;
  ik->find_field_from_offset(offset, false /* is_static */, &fd);
  bool is_volatile = fd.is_volatile();

  return is_volatile;
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
        v1.oop_val = oop(dram_v != NULL ? dram_v->nvm_header().fwd() : NULL);
        v2.oop_val = Raw::oop_load_in_heap_at(nvm, offset);
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  return v1.long_val == v2.long_val;
}

inline bool OurPersist::is_set_durableroot_annotation(oop klass_obj, ptrdiff_t offset) {
  assert(klass_obj != NULL, "");
  assert(klass_obj->klass()->is_instance_klass(), "");
  assert(((InstanceKlass*)klass_obj->klass())->is_mirror_instance_klass(), "");

#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
  return true;
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
  return false;
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  // Unimplements
  Unimplemented();
  return false;
}

inline Thread* OurPersist::responsible_thread(void* nvm_obj) {
  assert(nvmHeader::is_fwd(nvm_obj), "nvm_obj: %p", nvm_obj);

  return (Thread*)oop(nvm_obj)->nvm_header().fwd();
}

inline bool OurPersist::shade(oop obj, Thread* cur_thread) {
  void* nvm_obj = OurPersist::allocate_nvm(obj->size(), cur_thread);
  bool success = nvmHeader::cas_fwd(obj, nvm_obj);
  if (!success) {
    // TODO: nvm release
  }
  return success;
}

#endif // NVM_OURPERSIST_INLINE_HPP
#endif // OUR_PERSIST
