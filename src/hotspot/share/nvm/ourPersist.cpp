#ifdef OUR_PERSIST

#include "classfile/javaClasses.hpp"
#include "gc/nvm_card/nvmCardTableBarrierSet.hpp"
#include "gc/shared/cardTableBarrierSet.hpp"
#include "nvm/nvmBarrierSync.inline.hpp"
#include "nvm/nvmMacro.hpp"
#include "nvm/nvmWorkListStack.hpp"
#include "nvm/oops/nvmMirrorOop.hpp"
#include "nvm/oops/nvmOop.hpp"
#include "nvm/ourPersist.inline.hpp"
#include "oops/arrayKlass.hpp"
#include "oops/fieldStreams.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayKlass.hpp"
#include "runtime/fieldDescriptor.inline.hpp"
#include "runtime/signature.hpp"
#include "runtime/thread.hpp"
#include "utilities/globalDefinitions.hpp"

#ifdef ASSERT
#include "nvm/nvmDebug.hpp"
#endif // ASSERT

#include "runtime/handshake.hpp"
#include "runtime/interfaceSupport.inline.hpp" // thread_block_invm
#include "runtime/threadSMR.hpp" // java thread iterator
#include "utilities/hashtable.inline.hpp" // stack
#include "utilities/stack.inline.hpp" // stack

int OurPersist::_enable = our_persist_unknown;
#ifdef OURPERSIST_DURABLEROOTS_ALL_TRUE
int OurPersist::_started = our_persist_true;
#else // OURPERSIST_DURABLEROOTS_ALL_TRUE
int OurPersist::_started = our_persist_unknown;
#endif // OURPERSIST_DURABLEROOTS_ALL_TRUE

class ReplicateNotifyClosure : public HandshakeClosure {
protected:
  static inline int next_id{0};
  int _id;
public:
  ReplicateNotifyClosure(): 
    HandshakeClosure("Replicate notify"), _id (next_id++) {}

  void do_thread(Thread* target) {
    // printf("  sync %d (thread = %p)\n", _id, target);
  }
  int get_id() {
    return _id;
  }
};


void OurPersist::handshake() {
  assert(Thread::current()->is_Java_thread(), "must be");

  JavaThread* self = Thread::current()->as_Java_thread();

  ThreadInVMForHandshake th{self};
  assert(self->thread_state() == _thread_in_vm, "Thread not in expected state");

  ReplicateNotifyClosure rnc{};
  Handshake::execute_live(&rnc);

  // printf("thread %p sends handshake\n", (void*)self);

}

unsigned hash_for_oop(const oop& k) {
  // Specialized implementation for oop
  unsigned hash = (unsigned)(cast_from_oop<uintptr_t>(k));
  return hash ^ (hash >> 3);
}

class OopSet : public KVHashtable<oop, bool, mtInternal, hash_for_oop> {
private:
  int current_size = 0;  // Counter for the add() calls

public:
  OopSet(int table_size) : KVHashtable<oop, bool, mtInternal, hash_for_oop>(table_size) {}

  bool contains(oop key) {
    return KVHashtable<oop, bool, mtInternal, hash_for_oop>::lookup(key) != nullptr;
  }

  void insert(oop key) {
    assert(!contains(key), "only called in this case");
    KVHashtable<oop, bool, mtInternal, hash_for_oop>::add(key, true);
    current_size++;  // Increment the counter
  }

  int size() const {
    return current_size;
  }
};

class MakePersistentBase {

public:
  MakePersistentBase() = delete;
  MakePersistentBase(Thread* thread, int round, GrowableArray<Handle>* objs_marked):
    _thr{thread},
    _worklist{thread->nvm_work_list()},
    _barrier_sync{thread->nvm_barrier_sync()},
    _round{round},
    _st{nullptr},
    _objs_marked{objs_marked} {
    }

  void record(oop v) {
    _objs_marked->push(Handle(_thr, v));
  }
  int round() {
    return _round;
  }

protected:
  // TODO:
  // confirm we should skip or not when !OurPersist::is_target(v->klass())
  void try_push(oop v) {
    assert(_st != nullptr, "must be");
    if (v == nullptr || v->nvm_header().recoverable() || !OurPersist::is_target(v->klass())) {
      return;
    }

    // already pushed in this round
    if (_st->contains(v)) {
      return;
    }

    _worklist->push(v);
    _st->insert(v);

    assert(_st->contains(v), "must be");
  }

  // Iterate function for instance classes
  // TODO: try obj->iterate_oop
  void push_for_instance_class(oop obj, Klass* klass) {
    using Parent =  CardTableBarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP, NVMCardTableBarrierSet>;

    for (InstanceKlass* ik = (InstanceKlass*)klass; ik != nullptr; ik = (InstanceKlass*)ik->super()) {
      int field_count = ik->java_fields_count();
      for (int i = 0; i < field_count; i++) {
        if (accessFlags_from(ik->field_access_flags(i)).is_static()) {
          continue;
        }
        Symbol* field_signature = ik->field_signature(i);
        BasicType field_type = Signature::basic_type(field_signature);

        if (is_reference_type(field_type)) {
          int field_offset = ik->field_offset(i);

          oop child = Parent::oop_load_in_heap_at(obj, field_offset);
          assert(oopDesc::is_oop_or_null(child, true), "must be");
          try_push(child);
        }
      }
    }
  }

    // Iterate function for arrays
  void push_for_obj_array(oop obj, Klass* klass) {
    using Parent =  CardTableBarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY, NVMCardTableBarrierSet>;

    auto array_klass = static_cast<ObjArrayKlass*>(klass);
    BasicType array_type = array_klass->element_type();
    assert(is_reference_type(array_type), "must be");
    auto array_obj = static_cast<objArrayOop>(obj);
    int array_length = array_obj->length();

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      oop child = Parent::oop_load_in_heap_at(obj, field_offset);
      // Handle h(_thr, child);
      assert(oopDesc::is_oop_or_null(child, true), "must be");
      try_push(child);
    }
  }

  void push_oop_fields(oop obj) {
    Klass* klass = obj->klass();
    if (klass->is_instance_klass()) {
      push_for_instance_class(obj, klass);
    } else if (klass->is_objArray_klass()) {
      push_for_obj_array(obj, klass);
    } else if (klass->is_typeArray_klass()) {
      // no oop field
    } else {
      assert(false, "modify code");
    }
  }


  Thread*  _thr;
  NVMWorkListStack* _worklist;
  NVMBarrierSync* _barrier_sync;
  int _round;
  OopSet* _st;
  GrowableArray<Handle>* _objs_marked;

};


class MakePersistentMarkPhase: public MakePersistentBase {
public:
  MakePersistentMarkPhase(GrowableArray<Handle>* objs_marked): 
    MakePersistentBase{Thread::current(), 0, objs_marked} {
    }
  int process(oop obj) {
    OopSet _has_been_visited {1024 * 8};
    _st = &_has_been_visited;

    _round++;
    assert(_round < 6, "too many rounds");
    _n_shaded = 0;
    
    try_push(obj);
    while (!_worklist->is_empty()) {
        oop v = _worklist->pop();
        visit(v);
    }
    assert(_worklist->is_empty() && _n_shaded >= 0, "must be");

    return _n_shaded;
  }

private:
  // shade and push oop fields of this obj
  void visit(oop obj)  {
    if (!try_shade(obj)) {
      return;
    }
    push_oop_fields(obj);
  }
  
  // true if v is gray and shaded by this thread, 
  // where its children need to be pushed
  bool try_shade(oop v) {
    
    // not persistent
    nvmOop replica = v->nvm_header().fwd();
    if (replica == nullptr) {
      // not marked yet, mark it
      if (OurPersist::shade(v, _thr)) {
        // success
        _n_shaded++;
        replica = v->nvm_header().fwd();
        assert(replica != nullptr && replica->responsible_thread() == _thr, "must be");
        // shaded v successfully, copy v later
        record(v);
        OurPersist::add_dependent_obj_list(replica, _thr);
        return true;
      } else {
        // fail
        replica = v->nvm_header().fwd();
        assert(replica != nullptr && replica->responsible_thread() != _thr, "must be");
        _barrier_sync->add(v, replica, _thr);
        return false;
      }
    } else {
      // already marked
      if (replica->responsible_thread() == _thr) {
        return true;
      } else {
        _barrier_sync->add(v, replica, _thr);
        return false;
      }
    }
  }

  private:
  int _n_shaded;

};


class MakePersistentCopyPhase: public MakePersistentBase {
public:
  MakePersistentCopyPhase() = delete;
  MakePersistentCopyPhase(int marker_round, GrowableArray<Handle>* objs_marked):
    MakePersistentBase{Thread::current(), marker_round + 1, objs_marked} {
    }

  void process() {
    for (GrowableArrayIterator<Handle> it = _objs_marked->begin(); it != _objs_marked->end(); ++it) {
      visit((*it)());
    }
  }

private:
  // 1. obj.responsible_thread == _thr:
  //    copy all fields and push all oop fields
  // 2. obj.responsible_thread != _thr:
  //    push all oop fields
  void visit(oop obj)  {
    assert(obj != nullptr, "must be");
    assert(obj->nvm_header().fwd() != nullptr, "must be");
    assert(obj->nvm_header().fwd()->responsible_thread() == _thr, "must be");

   
    // repeat copying till success
    nvmHeader::lock(obj);
    Klass* klass = obj->klass();

    do {
      if (klass->is_instance_klass()) {
        iterate_instance_class(obj, klass);
      } else if (klass->is_objArray_klass()) {
        iterate_obj_array(obj, klass);
      } else if (klass->is_typeArray_klass()) {
        copy_type_array(obj, klass);
      } else {
        assert(false, "modify code");
      }

      OrderAccess::fence();

      // write back & sfence
      NVM_FLUSH_LOOP(obj->nvm_header().fwd(), obj->size() * HeapWordSize);

    } while (!OurPersist::copy_object_verify_step(obj, obj->nvm_header().fwd(), klass));

    nvmHeader::unlock(obj);
      
    
  }

  void copy_type_array(oop obj, Klass* klass) {
    TypeArrayKlass* ak = (TypeArrayKlass*)klass;
    typeArrayOop ao = (typeArrayOop)obj;
    BasicType array_type = ((ArrayKlass*)ak)->element_type();
    int array_length = ao->length();

    nvmOop o_rep = obj->nvm_header().fwd();
    assert(o_rep != nullptr, "must be");
    typeArrayOop(o_rep)->set_length(array_length);

    void* obj_ptr = OOP_TO_VOID(obj);
    int base_offset_in_bytes = typeArrayOopDesc::base_offset_in_bytes(array_type);
    int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
    assert(base_offset_in_bytes % HeapWordSize == 0, "");
    HeapWord* to   = (HeapWord*)((char*)o_rep + base_offset_in_bytes);
    HeapWord* from = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
    Copy::aligned_disjoint_words(from, to, obj->size() - base_offset_in_words);
  }

  void copy_oop(oop obj, oop o_rep, int offset, oop v, bool is_array) {

    nvmOop v_rep = [](oop v) -> nvmOop {
      if (v == nullptr || !OurPersist::is_target(v->klass())) {
        return nullptr;
      }
      // v shouldn't be white for a long time
      nvmOop replica = v->nvm_header().fwd();
      while (replica == nullptr) {
        replica = v->nvm_header().fwd();
      }
      assert(replica != nullptr, "must be");
      return replica;
    }(v);

    
    if (is_array) {
      using Raw = BarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY, NVMCardTableBarrierSet>;
      Raw::oop_store_in_heap_at((oop)o_rep, offset, (oop)v_rep);
    } else {
      using Raw = BarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP, NVMCardTableBarrierSet>;
      Raw::oop_store_in_heap_at((oop)o_rep, offset, (oop)v_rep);
    }



  } 

  // Iterate function for instance classes
  // TODO: use obj->iterate_oop
  void iterate_instance_class(oop obj, Klass* klass) {
    using Parent =  CardTableBarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP, NVMCardTableBarrierSet>;

    nvmOop o_rep = obj->nvm_header().fwd();
    assert(o_rep != nullptr, "must be");

    for (InstanceKlass* ik = (InstanceKlass*)klass; ik != nullptr; ik = (InstanceKlass*)ik->super()) {
      int field_count = ik->java_fields_count();
      for (int i = 0; i < field_count; i++) {
        if (accessFlags_from(ik->field_access_flags(i)).is_static()) {
          continue;
        }
        Symbol* field_signature = ik->field_signature(i);
        BasicType field_type = Signature::basic_type(field_signature);
        int field_offset = ik->field_offset(i);

        
        if (is_reference_type(field_type)) {
          oop child = Parent::oop_load_in_heap_at(obj, field_offset);
          assert(oopDesc::is_oop_or_null(child, true), "must be");
          copy_oop(obj, (oop)o_rep, field_offset, child, false);
        } else {
          // copy this field
          OurPersist::copy_dram_to_nvm(obj, (oop)o_rep, field_offset, field_type);
        }
      }
    }
  }

    // Iterate object array
  void iterate_obj_array(oop obj, Klass* klass) {
    using Parent =  CardTableBarrierSet::AccessBarrier<MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY, NVMCardTableBarrierSet>;

    auto array_klass = static_cast<ObjArrayKlass*>(klass);
    BasicType array_type = array_klass->element_type();
    assert(is_reference_type(array_type), "must be");
    auto array_obj = static_cast<objArrayOop>(obj);
    int array_length = array_obj->length();

    nvmOop o_rep = obj->nvm_header().fwd();
    assert(o_rep != nullptr, "must be");
    objArrayOop(o_rep)->set_length(array_length);

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      oop child = Parent::oop_load_in_heap_at(obj, field_offset);
      assert(oopDesc::is_oop_or_null(child, true), "must be");
      copy_oop(obj, (oop)o_rep, field_offset, child, true);
    }
  }

};

class BarrierSyncMark {
  public:
  static inline int cnt = 0;
  BarrierSyncMark() {
    // printf("enter %d\n", ++cnt);
    _thr->nvm_barrier_sync()->init();
  }
  ~BarrierSyncMark() {
    _thr->nvm_barrier_sync()->sync();
    OurPersist::clear_responsible_thread(_thr);
    // printf("exit %d\n", cnt);
  }

  private:
  Thread* _thr{Thread::current()};

};

void OurPersist::ensure_recoverable(Handle h_obj) {
  assert(!h_obj()->nvm_header().recoverable() && OurPersist::is_target(h_obj()->klass()), "condition to enter the function");

  ResourceMark rm;
  
  BarrierSyncMark bsm;

  GrowableArray<Handle> objs_marked;
  assert(objs_marked.length() == 0, "sanity check");

  MakePersistentMarkPhase marker{&objs_marked};

  marker.process(h_obj());
  
  if (objs_marked.length() == 0) {
    return;
  }

  do {
    handshake();
  } while (marker.process(h_obj()) > 0);

  MakePersistentCopyPhase copier{marker.round(), &objs_marked};
  copier.process();
}


void OurPersist::ensure_recoverable(oop obj) {
  if (obj->nvm_header().recoverable()) {
    return;
  }
  if (!OurPersist::is_target(obj->klass())) {
    return;
  }

  auto thread = Thread::current();

  HandleMark hm(thread);
  Handle h_obj = Handle(thread, obj);

  OurPersist::ensure_recoverable(h_obj);

// #ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
//   ShouldNotReachHere();
// #endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

// #ifdef NO_ENSURE_RECOVERABLE
//   return;
// #endif // NO_ENSURE_RECOVERABLE

//   if (obj->nvm_header().recoverable()) {
//     return;
//   }
//   if (!OurPersist::is_target(obj->klass())) {
//     return;
//   }

//   Thread* cur_thread = Thread::current();
//   NVMWorkListStack* worklist = cur_thread->nvm_work_list();
//   NVMBarrierSync* barrier_sync = cur_thread->nvm_barrier_sync();
//   barrier_sync->init();

//   bool success = OurPersist::shade(obj, cur_thread);
//   nvmOop nvm_obj = obj->nvm_header().fwd();
//   assert(nvm_obj != NULL, "");
//   if (!success) {
//     assert(nvm_obj->responsible_thread() != cur_thread, "");
//     barrier_sync->add(obj, nvm_obj, cur_thread);
//     barrier_sync->sync();
//     return;
//   }

//   assert(nvm_obj == obj->nvm_header().fwd(),
//          "nvm_obj: %p, fwd: %p", nvm_obj, obj->nvm_header().fwd());
//   assert(nvm_obj->responsible_thread() == Thread::current(),
//          "fwd: %p", obj->nvm_header().fwd());
//   worklist->add(obj);

//   NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_call_ensure_recoverable();)

//   while (worklist->empty() == false) {
//     oop cur_obj = worklist->remove();
//     nvmOop cur_nvm_obj = cur_obj->nvm_header().fwd();

//     NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_persistent_obj(cur_obj->size());)

//     OurPersist::add_dependent_obj_list(cur_nvm_obj, cur_thread);
//     OurPersist::copy_object(cur_obj);
//   }

//   // sfence
//   NVM_FENCE

//   barrier_sync->sync();
//   OurPersist::clear_responsible_thread(cur_thread);
}

void OurPersist::copy_object_copy_step(oop obj, nvmOop nvm_obj, Klass* klass,
                                       NVMWorkListStack* worklist, NVMBarrierSync* barrier_sync,
                                       Thread* cur_thread) {
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
          nvmOop nvm_v = NULL;

          if (v != NULL && OurPersist::is_target(v->klass())) {
            if (v->nvm_header().recoverable()) {
              nvm_v = v->nvm_header().fwd();
            } else {
              bool success = OurPersist::shade(v, cur_thread);
              nvm_v = v->nvm_header().fwd();
              assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
              if (success) {
                assert(nvm_v->responsible_thread() == Thread::current(), "fwd: %p", nvm_v);
                worklist->add(v);
              } else {
                barrier_sync->add(v, nvm_v, cur_thread);
              }
            }
          }
          Raw::oop_store_in_heap_at((oop)nvm_obj, field_offset, (oop)nvm_v);
        } else {
#ifndef OURPERSIST_INSTANCE_MAYBE_FASTCOPY
          OurPersist::copy_dram_to_nvm(obj, (oop)nvm_obj, field_offset, field_type);
#endif // OURPERSIST_INSTANCE_MAYBE_FASTCOPY
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

    objArrayOop(nvm_obj)->set_length(array_length);

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY;
      typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
      typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;
      oop v = Parent::oop_load_in_heap_at(obj, field_offset);

      nvmOop nvm_v = NULL;
      if (v != NULL && OurPersist::is_target(v->klass())) {
        if (v->nvm_header().recoverable()) {
          nvm_v = v->nvm_header().fwd();
        } else {
          bool success = OurPersist::shade(v, cur_thread);
          nvm_v = v->nvm_header().fwd();
          assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
          if (success) {
            assert(nvm_v->responsible_thread() == Thread::current(), "fwd: %p", nvm_v);
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

    typeArrayOop(nvm_obj)->set_length(array_length);

#ifdef OURPERSIST_TYPEARRAY_SLOWCOPY
    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
      OurPersist::copy_dram_to_nvm(obj, (oop)nvm_obj, field_offset, array_type, true);
    }
#else // OURPERSIST_TYPEARRAY_SLOWCOPY
    void* obj_ptr = OOP_TO_VOID(obj);
    int base_offset_in_bytes = typeArrayOopDesc::base_offset_in_bytes(array_type);
    int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
    assert(base_offset_in_bytes % HeapWordSize == 0, "");
    HeapWord* to   = (HeapWord*)((char*)nvm_obj + base_offset_in_bytes);
    HeapWord* from = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
    Copy::aligned_disjoint_words(from, to, obj->size() - base_offset_in_words /* word size */);
#endif // OURPERSIST_TYPEARRAY_SLOWCOPY
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  // end "for f in obj.fields"
}

bool OurPersist::copy_object_verify_step(oop obj, nvmOop nvm_obj, Klass* klass) {
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

    assert(objArrayOop(nvm_obj)->length() == array_length, "length mismatch");

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      bool same = OurPersist::cmp_dram_and_nvm(obj, (oop)nvm_obj, field_offset, T_OBJECT, false);
      if (!same) {
        return false;
      }
    }
  } else if (klass->is_typeArray_klass()) {
    TypeArrayKlass* ak = (TypeArrayKlass*)klass;
    BasicType array_type = ((ArrayKlass*)ak)->element_type();

    assert(typeArrayOop(nvm_obj)->length() == typeArrayOop(obj)->length(), "length mismatch");
#ifdef OURPERSIST_TYPEARRAY_SLOWCOPY
    typeArrayOop ao = (typeArrayOop)obj;
    int array_length = ao->length();
    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = arrayOopDesc::base_offset_in_bytes(array_type) + type2aelembytes(array_type) * i;
      bool same = OurPersist::cmp_dram_and_nvm(obj, (oop)nvm_obj, field_offset, array_type, true);
      if (!same) {
        return false;
      }
    }
#else // OURPERSIST_TYPEARRAY_SLOWCOPY
    void* obj_ptr = OOP_TO_VOID(obj);
    int base_offset_in_bytes = typeArrayOopDesc::base_offset_in_bytes(array_type);
    int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
    assert(base_offset_in_bytes % HeapWordSize == 0, "");
    HeapWord* to   = (HeapWord*)((char*)nvm_obj + base_offset_in_bytes);
    HeapWord* from = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
    return memcmp(from, to, (obj->size() - base_offset_in_words) * HeapWordSize) == 0;
#endif // OURPERSIST_TYPEARRAY_SLOWCOPY
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  // end "for f in obj.fields"
  return true;
}

void OurPersist::copy_object(oop obj) {
  assert(obj != NULL, "obj: %p", OOP_TO_VOID(obj));
  assert(obj->nvm_header().fwd() != NULL, "fwd: %p", obj->nvm_header().fwd());
  assert(obj->nvm_header().fwd()->responsible_thread() == Thread::current(),
         "fwd: %p", obj->nvm_header().fwd());

  Klass* klass = obj->klass();
  nvmOop nvm_obj = obj->nvm_header().fwd();
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

bool OurPersist::shade(oop obj, Thread* cur_thread) {
  nvmOop nvm_obj = OurPersist::allocate_nvm(obj->size(), cur_thread);
  nvmMirrorOop nvm_mirror = nvmMirrorOop(obj->klass()->java_mirror()->nvm_header().fwd());
  nvm_obj->set_klass(nvm_mirror);
  bool success = nvmHeader::cas_fwd(obj, nvm_obj);
  if (!success) {
    // TODO: nvm release
  }

  assert(nvm_obj->responsible_thread() == cur_thread, "");
  assert(success == (obj->nvm_header().fwd() == nvm_obj), "");

  return success;
}

#endif // OUR_PERSIST
