#ifdef OUR_PERSIST
#ifdef ASSERT

#include "gc/nvm_card/nvmCardTableBarrierSet.inline.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "nvm/nvmDebug.hpp"
#include "nvm/nvmMacro.hpp"
#include "oops/oop.inline.hpp"
#include "oops/klass.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/fieldDescriptor.inline.hpp"

int NVMDebug::obj_cnt = 0;

void NVMDebug::print_native_stack() {
  char buf[2000];
  VMError::print_native_stack(tty, os::current_frame(), Thread::current(), buf, sizeof(buf));
}

void NVMDebug::print_klass_id(Klass* k) {
  int k_id = k->id();
  assert(InstanceKlassID <= k_id && k_id <= ObjArrayKlassID, "");
  if (k_id == InstanceKlassID)            tty->print("InstanceKlassID\n");
  if (k_id == InstanceRefKlassID)         tty->print("InstanceRefKlassID\n");
  if (k_id == InstanceMirrorKlassID)      tty->print("InstanceMirrorKlassID\n");
  if (k_id == InstanceClassLoaderKlassID) tty->print("InstanceClassLoaderKlassID\n");
  if (k_id == TypeArrayKlassID)           tty->print("TypeArrayKlassID\n");
  if (k_id == ObjArrayKlassID)            tty->print("ObjArrayKlassID\n");
}

void NVMDebug::print_decorators(DecoratorSet ds) {
  tty->print("[decorators] %" PRIu64 "\n", ds);
  if (ds & DECORATORS_NONE) tty->print("DECORATORS_NONE\n");
  if (ds & INTERNAL_CONVERT_COMPRESSED_OOP) tty->print("INTERNAL_CONVERT_COMPRESSED_OOP\n");
  if (ds & INTERNAL_VALUE_IS_OOP) tty->print("INTERNAL_VALUE_IS_OOP\n");
  if (ds & INTERNAL_RT_USE_COMPRESSED_OOPS) tty->print("INTERNAL_RT_USE_COMPRESSED_OOPS\n");
  if (ds & MO_UNORDERED) tty->print("MO_UNORDERED\n");
  if (ds & MO_RELAXED) tty->print("MO_RELAXED\n");
  if (ds & MO_ACQUIRE) tty->print("MO_ACQUIRE\n");
  if (ds & MO_RELEASE) tty->print("MO_RELEASE\n");
  if (ds & MO_SEQ_CST) tty->print("MO_SEQ_CST\n");
  if (ds & AS_RAW) tty->print("AS_RAW\n");
  if (ds & AS_NO_KEEPALIVE) tty->print("AS_NO_KEEPALIVE\n");
  if (ds & AS_NORMAL) tty->print("AS_NORMAL\n");
  if (ds & ON_STRONG_OOP_REF) tty->print("ON_STRONG_OOP_REF\n");
  if (ds & ON_WEAK_OOP_REF) tty->print("ON_WEAK_OOP_REF\n");
  if (ds & ON_PHANTOM_OOP_REF) tty->print("ON_PHANTOM_OOP_REF\n");
  if (ds & ON_UNKNOWN_OOP_REF) tty->print("ON_UNKNOWN_OOP_REF\n");
  if (ds & IN_HEAP) tty->print("IN_HEAP\n");
  if (ds & IN_NATIVE) tty->print("IN_NATIVE\n");
  if (ds & IS_ARRAY) tty->print("IS_ARRAY\n");
  if (ds & IS_DEST_UNINITIALIZED) tty->print("IS_DEST_UNINITIALIZED\n");
  if (ds & ARRAYCOPY_CHECKCAST) tty->print("ARRAYCOPY_CHECKCAST\n");
  if (ds & ARRAYCOPY_DISJOINT) tty->print("ARRAYCOPY_DISJOINT\n");
  if (ds & ARRAYCOPY_ARRAYOF) tty->print("ARRAYCOPY_ARRAYOF\n");
  if (ds & ARRAYCOPY_ATOMIC) tty->print("ARRAYCOPY_ATOMIC\n");
  if (ds & ARRAYCOPY_ALIGNED) tty->print("ARRAYCOPY_ALIGNED\n");
  if (ds & ACCESS_READ) tty->print("ACCESS_READ\n");
  if (ds & ACCESS_WRITE) tty->print("ACCESS_WRITE\n");
  // == OurPersist Decorators ==
  if (ds & OURPERSIST_BS_ASM) tty->print("OURPERSIST_BS_ASM\n");
  if (ds & OURPERSIST_DURABLE_ANNOTATION) tty->print("OURPERSIST_DURABLE_ANNOTATION\n");
  if (ds & OURPERSIST_NOT_DURABLE_ANNOTATION) tty->print("OURPERSIST_NOT_DURABLE_ANNOTATION\n");
  if (ds & OURPERSIST_IS_STATIC) tty->print("OURPERSIST_IS_STATIC\n");
  if (ds & OURPERSIST_IS_NOT_STATIC) tty->print("OURPERSIST_IS_NOT_STATIC\n");
  if (ds & OURPERSIST_IS_VOLATILE) tty->print("OURPERSIST_IS_VOLATILE\n");
  if (ds & OURPERSIST_IS_NOT_VOLATILE) tty->print("OURPERSIST_IS_NOT_VOLATILE\n");
  tty->print("--\n");
}

void NVMDebug::print_dram_and_nvm_val(oop dram_obj, ptrdiff_t offset, BasicType type) {
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
  } v1, v2, v3;
  v1.long_val = 0L;
  v2.long_val = 0L;
  v3.long_val = 0L;

  void* nvm_fwd = dram_obj->nvm_header().fwd();
  oop nvm_obj = nvmHeader::is_fwd(nvm_fwd) ? oop(nvm_fwd) : NULL;

  switch(type) {
    case T_BYTE:
      {
        v1.byte_val = Parent::load_in_heap_at<jbyte>(dram_obj, offset);
        if (nvm_obj != NULL) v2.byte_val = Raw::load_in_heap_at<jbyte>(nvm_obj, offset);
        break;
      }
    case T_CHAR:
      {
        v1.char_val = Parent::load_in_heap_at<jchar>(dram_obj, offset);
        if (nvm_obj != NULL) v2.char_val = Raw::load_in_heap_at<jchar>(nvm_obj, offset);
        break;
      }
    case T_DOUBLE:
      {
        v1.double_val = Parent::load_in_heap_at<jdouble>(dram_obj, offset);
        if (nvm_obj != NULL) v2.double_val = Raw::load_in_heap_at<jdouble>(nvm_obj, offset);
        break;
      }
    case T_FLOAT:
      {
        v1.float_val = Parent::load_in_heap_at<jfloat>(dram_obj, offset);
        if (nvm_obj != NULL) v2.float_val = Raw::load_in_heap_at<jfloat>(nvm_obj, offset);
        break;
      }
    case T_INT:
      {
        v1.int_val = Parent::load_in_heap_at<jint>(dram_obj, offset);
        if (nvm_obj != NULL) v2.int_val = Raw::load_in_heap_at<jint>(nvm_obj, offset);
        break;
      }
    case T_LONG:
      {
        v1.long_val = Parent::load_in_heap_at<jlong>(dram_obj, offset);
        if (nvm_obj != NULL) v2.long_val = Raw::load_in_heap_at<jlong>(nvm_obj, offset);
        break;
      }
    case T_SHORT:
      {
        v1.short_val = Parent::load_in_heap_at<jshort>(dram_obj, offset);
        if (nvm_obj != NULL) v2.short_val = Raw::load_in_heap_at<jshort>(nvm_obj, offset);
        break;
      }
    case T_BOOLEAN:
      {
        v1.bool_val = Parent::load_in_heap_at<jboolean>(dram_obj, offset) & 0x1;
        if (nvm_obj != NULL) v2.bool_val = Raw::load_in_heap_at<jboolean>(nvm_obj, offset) & 0x1;
        break;
      }
    case T_OBJECT:
    case T_ARRAY:
      {
        oop dram_v = Parent::oop_load_in_heap_at(dram_obj, offset);
        v1.oop_val = dram_v;
        if (nvm_obj != NULL) v2.oop_val = Raw::oop_load_in_heap_at(nvm_obj, offset);
        v3.oop_val = oop(dram_v != NULL ? dram_v->nvm_header().fwd() : NULL);
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }

  if (!is_reference_type(type)) {
    tty->print("dram_obj[%ld]: %ld", offset, v1.long_val);
    if (nvm_obj != NULL) tty->print(", nvm_obj[%ld]: %ld", offset, v2.long_val);
    tty->cr();
  } else {
    tty->print("dram_obj[%ld]: %p(nvm_fwd: %p)", offset, v1.oop_val, v3.oop_val);
    if (nvm_obj != NULL) tty->print(", nvm_obj[%ld]: %p", offset, v2.oop_val);
    tty->cr();
  }
}

void NVMDebug::print_dram_and_nvm_obj(oop dram_obj) {
  Klass* klass = dram_obj->klass();
  bool has_same_field;

  if (klass->is_instance_klass()) {
    InstanceKlass* ik = (InstanceKlass*)klass;
    while (ik != NULL) {
      int cnt = ik->java_fields_count();
      for (int i = 0; i < cnt; i++) {
        AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
        Symbol* field_sig = ik->field_signature(i);
        BasicType field_type = Signature::basic_type(field_sig);
        ptrdiff_t field_offset = ik->field_offset(i);

        if (field_flags.is_static()) {
          continue;
        }

        NVMDebug::print_dram_and_nvm_val(dram_obj, field_offset, field_type);
      }
      ik = (InstanceKlass*)ik->super();
    }
  } else if (klass->is_objArray_klass()) {
    ObjArrayKlass* oak = (ObjArrayKlass*)klass;
    objArrayOop oao = (objArrayOop)dram_obj;
    BasicType array_type = ((ArrayKlass*)oak)->element_type();
    int array_length = oao->length();

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      NVMDebug::print_dram_and_nvm_val(dram_obj, field_offset, array_type);
    }
  } else if (klass->is_typeArray_klass()) {
    TypeArrayKlass* tak = (TypeArrayKlass*)klass;
    typeArrayOop tao = (typeArrayOop)dram_obj;
    BasicType array_type = ((ArrayKlass*)tak)->element_type();
    int array_length = tao->length();

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
      NVMDebug::print_dram_and_nvm_val(dram_obj, field_offset, array_type);
    }
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
}

bool NVMDebug::cmp_dram_and_nvm_val(oop dram_obj, oop nvm_obj, ptrdiff_t offset,
                                    BasicType type, AccessFlags field_flags) {
  const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP;
  typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
  typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;

  assert(nvmHeader::is_fwd(nvm_obj), "");

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
        v1.byte_val = Parent::load_in_heap_at<jbyte>(dram_obj, offset);
        v2.byte_val = Raw::load_in_heap_at<jbyte>(nvm_obj, offset);
        break;
      }
    case T_CHAR:
      {
        v1.char_val = Parent::load_in_heap_at<jchar>(dram_obj, offset);
        v2.char_val = Raw::load_in_heap_at<jchar>(nvm_obj, offset);
        break;
      }
    case T_DOUBLE:
      {
        v1.double_val = Parent::load_in_heap_at<jdouble>(dram_obj, offset);
        v2.double_val = Raw::load_in_heap_at<jdouble>(nvm_obj, offset);
        break;
      }
    case T_FLOAT:
      {
        v1.float_val = Parent::load_in_heap_at<jfloat>(dram_obj, offset);
        v2.float_val = Raw::load_in_heap_at<jfloat>(nvm_obj, offset);
        break;
      }
    case T_INT:
      {
        v1.int_val = Parent::load_in_heap_at<jint>(dram_obj, offset);
        v2.int_val = Raw::load_in_heap_at<jint>(nvm_obj, offset);
        break;
      }
    case T_LONG:
      {
        v1.long_val = Parent::load_in_heap_at<jlong>(dram_obj, offset);
        v2.long_val = Raw::load_in_heap_at<jlong>(nvm_obj, offset);
        break;
      }
    case T_SHORT:
      {
        v1.short_val = Parent::load_in_heap_at<jshort>(dram_obj, offset);
        v2.short_val = Raw::load_in_heap_at<jshort>(nvm_obj, offset);
        break;
      }
    case T_BOOLEAN:
      {
        v1.bool_val = Parent::load_in_heap_at<jboolean>(dram_obj, offset) & 0x1;
        v2.bool_val = Raw::load_in_heap_at<jboolean>(nvm_obj, offset) & 0x1;
        break;
      }
    case T_OBJECT:
    case T_ARRAY:
      {
        oop dram_v = Parent::oop_load_in_heap_at(dram_obj, offset);
        v1.oop_val = oop(dram_v != NULL ? dram_v->nvm_header().fwd() : NULL);
        v2.oop_val = Raw::oop_load_in_heap_at(nvm_obj, offset);
        if (v1.long_val != v2.long_val) {
          tty->print("dram_obj: %p, is_recoverable: %d, is_target: %d\n",
            OOP_TO_VOID(dram_obj), dram_obj->nvm_header().recoverable(),
            OurPersist::is_target(dram_obj->klass()));
          tty->print("dram_v: %p, is_recoverable: %d, is_target: %d\n",
            OOP_TO_VOID(dram_v), dram_v->nvm_header().recoverable(), OurPersist::is_target(dram_v->klass()));
          tty->print("nvm_obj: %p\n", OOP_TO_VOID(nvm_obj));
          tty->print("%p != %p\n", v1.oop_val, v2.oop_val);

          NVMDebug::print_dram_and_nvm_obj(dram_obj);
        }
        break;
      }
    default:
      report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  return v1.long_val == v2.long_val;
}

bool NVMDebug::cmp_dram_and_nvm_obj_during_gc(oop dram_obj) {
  assert(!dram_obj->nvm_header().is_locked(), "");

  oop nvm_obj = oop(dram_obj->nvm_header().fwd());
  if (nvm_obj == NULL) {
    // tty->print("doesn't have nvm copy.\n");
    return true;
  }

  NVMDebug::obj_cnt++;

  assert(nvmHeader::is_fwd(nvm_obj), "");
  assert(dram_obj->nvm_header().recoverable(), "");

  Klass* klass = dram_obj->klass();
  bool has_same_field;

  if (klass->is_instance_klass()) {
    // tty->print("InstanceKlass\n");

    if (klass->id() == InstanceMirrorKlassID) {
      // tty->print("ik: mirror klass.\n");
      return true;
    }

    InstanceKlass* ik = (InstanceKlass*)klass;
    while (ik != NULL) {
      int cnt = ik->java_fields_count();
      for (int i = 0; i < cnt; i++) {
        AccessFlags field_flags = accessFlags_from(ik->field_access_flags(i));
        Symbol* field_sig = ik->field_signature(i);
        BasicType field_type = Signature::basic_type(field_sig);
        ptrdiff_t field_offset = ik->field_offset(i);

        if (field_flags.is_static()) {
          continue;
        }

        has_same_field = NVMDebug::cmp_dram_and_nvm_val(dram_obj, nvm_obj, field_offset,
                                                        field_type, field_flags);
        if (has_same_field) {
          // tty->print("ik: has same field.\n");
          // tty->print("field name: %s\n", field_sig->as_klass_external_name());
        } else {
          char str[100];
          tty->print("ik: doesn't have same field.\n");
          tty->print("ik: name: %s, offset: %ld\n", ik->name()->as_C_string(str, 100), field_offset);
          tty->print("ik: dram_obj is_recoverable: %d\n", dram_obj->nvm_header().recoverable());
          tty->print("ik: dram_obj is_target: %d\n", OurPersist::is_target(dram_obj->klass()));
          return false;
        }
      }
      ik = (InstanceKlass*)ik->super();
    }
  } else if (klass->is_objArray_klass()) {
    // tty->print("ObjArrayKlass\n");

    ObjArrayKlass* oak = (ObjArrayKlass*)klass;
    objArrayOop oao = (objArrayOop)dram_obj;
    BasicType array_type = ((ArrayKlass*)oak)->element_type();
    int array_length = oao->length();

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      has_same_field = NVMDebug::cmp_dram_and_nvm_val(dram_obj, nvm_obj, field_offset, T_OBJECT, AccessFlags());
      if (has_same_field) {
        // tty->print("oak: has same field.\n");
      } else {
        char str[100];
        tty->print("oak: doesn't have same field.\n");
        tty->print("oak: name: %s, obj: %p, offset: %ld, size: %d\n", oak->name()->as_C_string(str, 100), OOP_TO_VOID(dram_obj), field_offset, dram_obj->size());
        return false;
      }
    }
  } else if (klass->is_typeArray_klass()) {
    // tty->print("TypeArrayKlass\n");

    TypeArrayKlass* tak = (TypeArrayKlass*)klass;
    typeArrayOop tao = (typeArrayOop)dram_obj;
    BasicType array_type = ((ArrayKlass*)tak)->element_type();
    int array_length = tao->length();

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
      has_same_field = NVMDebug::cmp_dram_and_nvm_val(dram_obj, nvm_obj, field_offset, array_type, AccessFlags());
      if (has_same_field) {
        // tty->print("tak: has same field.\n");
      } else {
        char str[100];
        tty->print("tak: doesn't have same field.\n");
        tty->print("tak: name: %s, obj: %p, offset: %ld, size: %d\n", tak->name()->as_C_string(str, 100), OOP_TO_VOID(dram_obj), field_offset, dram_obj->size());
        return false;
      }
    }
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }

  return true;
}

#endif // ASSERT
#endif // OUR_PERSIST
