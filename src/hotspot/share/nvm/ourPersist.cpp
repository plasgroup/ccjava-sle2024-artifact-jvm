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
  #ifdef ASSERT
  #ifdef NVM_COUNTER
  NVMCounter::inc_handshake();
  #endif
  #endif

  #ifdef OUR_PERSIST_DISABLE_HANDSHAKE
  return;
  #endif
  assert(Thread::current()->is_Java_thread(), "must be");

  JavaThread* self = Thread::current()->as_Java_thread();

  ThreadInVMForHandshake th{self};
  assert(self->thread_state() == _thread_in_vm, "Thread not in expected state");

  ReplicateNotifyClosure rnc{};
  Handshake::execute_live(&rnc);

  // printf("thread %p sends handshake\n", (void*)self);

}

// unsigned hash_for_oop(const oop& k) {
//   // Specialized implementation for oop
//   unsigned hash = (unsigned)(cast_from_oop<uintptr_t>(k));
//   return hash ^ (hash >> 3);
// }

// class OopSet : public KVHashtable<oop, bool, mtInternal, hash_for_oop> {
// private:
//   int current_size = 0;  // Counter for the add() calls

// public:
//   OopSet(int table_size) : KVHashtable<oop, bool, mtInternal, hash_for_oop>(table_size) {}

//   bool contains(oop key) {
//     return KVHashtable<oop, bool, mtInternal, hash_for_oop>::lookup(key) != nullptr;
//   }

//   void insert(oop key) {
//     assert(!contains(key), "only called in this case");
//     KVHashtable<oop, bool, mtInternal, hash_for_oop>::add(key, true);
//     current_size++;  // Increment the counter
//   }

//   int size() const {
//     return current_size;
//   }
// };

class MakePersistentBase {

public:
  MakePersistentBase() = delete;
  
  explicit MakePersistentBase(Thread* thread):
    _thr{thread},
    _worklist{thread->nvm_work_list()},
    _barrier_sync{thread->nvm_barrier_sync()},
    _objs_marked{thread->marked_objects_list()} {
    }

  void record(oop v) {
    _objs_marked->push(Handle(_thr, v));
  }

protected:
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
  Thread*  _thr;
  NVMWorkListStack* _worklist;
  NVMBarrierSync* _barrier_sync;
  GrowableArray<Handle>* _objs_marked;

};


class MakePersistentMarkPhase: public MakePersistentBase {
public:
  MakePersistentMarkPhase(): 
    MakePersistentBase{Thread::current()} {
    }

  int process(oop obj) {
    assert(_objs_marked->length() == 0, "must be");

    _n_shaded = 0;
    
    try_shade_and_push(obj);

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
    Klass* klass = obj->klass();
    
    assert(obj->nvm_header().fwd() != nullptr, "sanity check");
    assert(OurPersist::is_target(klass), "sanity check");
    
    if (klass->is_instance_klass()) {
      iterate_instance_class(obj, klass);
    } else if (klass->is_objArray_klass()) {
      iterate_obj_array(obj, klass);
    } else if (klass->is_typeArray_klass()) {
      #ifdef OUR_PERSIST_SINGLE_FENCE
      copy_type_array(obj, klass);
      #endif
    } else {
      assert(false, "modify code");
    }
    #ifdef OUR_PERSIST_SINGLE_FENCE
    NVM_FLUSH_LOOP(obj->nvm_header().fwd(), obj->size() * HeapWordSize);
    #endif
  }
  // returns true iff v gets shaded by this thread in this invocation
  // then shade and push its children
  bool try_shade(oop v) {
    nvmOop replica = v->nvm_header().fwd();

    // gray the white object
    
    if (replica != nullptr || !OurPersist::shade(v, _thr)) {
      // fail
      replica = v->nvm_header().fwd();
      assert(replica != nullptr, "must be");
      _barrier_sync->add(v, replica, _thr);
      return false;
    }

    // succeed
    _n_shaded++;
    replica = v->nvm_header().fwd();
    assert(replica != nullptr && replica->responsible_thread() == _thr, "must be");
    // shaded v successfully, verify v later
    record(v);
    OurPersist::add_dependent_obj_list(replica, _thr);
    return true;
  }

  void try_shade_and_push(oop v) {
    if (v == nullptr || v->nvm_header().recoverable() || !OurPersist::is_target(v->klass())) {
      return;
    }

    if (try_shade(v)) {
      _worklist->push(v);
    }

    assert(v->nvm_header().fwd() != nullptr, "sanity check");
  }


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
          try_shade_and_push(child);
        #ifdef OUR_PERSIST_SINGLE_FENCE
          copy_oop(obj, (oop)o_rep, field_offset, child, false);
        } else {
          // copy this field
          OurPersist::copy_dram_to_nvm(obj, (oop)o_rep, field_offset, field_type);
        }
        #else
        }
        #endif
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
      try_shade_and_push(child);
      #ifdef OUR_PERSIST_SINGLE_FENCE
      copy_oop(obj, (oop)o_rep, field_offset, child, true);
      #endif
    }
  }
  
  
  

  private:
  int _n_shaded;

};


class MakePersistentCopyPhase: public MakePersistentBase {
public:
  MakePersistentCopyPhase():
    MakePersistentBase{Thread::current()} {
    }

  void process() {
    assert(_objs_marked->length() > 0, "must be");
    assert(_worklist->length() == 0, "sanity check");

    for (GrowableArrayIterator<Handle> it = _objs_marked->begin(); it != _objs_marked->end(); ++it) {
      visit((*it)());
    }

    _objs_marked->clear();
  }

private:
  // verify all fields, ensure consistency and oop field have replicas
  // if fail, push into worklist to recopy
  void visit(oop obj)  {
    assert(obj != nullptr, "must be");
    assert(obj->nvm_header().fwd() != nullptr, "must be");
    assert(obj->nvm_header().fwd()->responsible_thread() == _thr, "must be");
    assert(!obj->nvm_header().recoverable(), "must be");
    assert(OurPersist::is_target(obj->klass()), "sanity check");
   
    nvmHeader::lock(obj);
    Klass* klass = obj->klass();

    // if we do not use the single-fence version, i.e, we do not copy in mark phase
    // then we copy in this phase
    // and we add mfence per object
    #ifndef OUR_PERSIST_SINGLE_FENCE
    if (klass->is_instance_klass()) {
      iterate_instance_class(obj, klass);
    } else if (klass->is_objArray_klass()) {
      iterate_obj_array(obj, klass);
    } else if (klass->is_typeArray_klass()) {
      copy_type_array(obj, klass);
    } else {
      assert(false, "modify code");
    }

    // clwb & sfence
    NVM_WRITEBACK_LOOP(obj->nvm_header().fwd(), obj->size() * HeapWordSize);

    #endif


    if (!OurPersist::copy_object_verify_step(obj, obj->nvm_header().fwd(), klass)) {
      _worklist->push(obj);
      #ifdef NVM_COUNTER
      NVMCounter::inc_fail();
      #endif
    }

    nvmHeader::unlock(obj);
    
  }

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

  MakePersistentMarkPhase marker{};
  MakePersistentCopyPhase copier{};

  NVMWorkListStack* wl = Thread::current()->nvm_work_list();
  assert(wl->is_empty(), "sanity check");

  do {
    int n_shaded = marker.process(h_obj());
    
    if (n_shaded == 0) {
      break;
    }
    #ifdef OUR_PERSIST_SINGLE_FENCE
    NVM_FENCE
    #endif
    handshake();
    copier.process();
    
  } while (!wl->is_empty());

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


void OurPersist::ensure_recoverable_thread_local(oop obj) {
  assert(!obj->nvm_header().recoverable(), "precondition");
  assert(OurPersist::is_target(obj->klass()), "precondition");
  auto thread = Thread::current();

#ifdef OURPERSIST_DURABLEROOTS_ALL_FALSE
  ShouldNotReachHere();
#endif // OURPERSIST_DURABLEROOTS_ALL_FALSE

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();

  bool success = OurPersist::shade(obj, cur_thread);
  assert(success, "must be since thread-local");
  nvmOop nvm_obj = obj->nvm_header().fwd();
  assert(nvm_obj != NULL, "");
  assert(nvm_obj == obj->nvm_header().fwd(),
         "nvm_obj: %p, fwd: %p", nvm_obj, obj->nvm_header().fwd());
  assert(nvm_obj->responsible_thread() == Thread::current(),
         "fwd: %p", obj->nvm_header().fwd());
  worklist->add(obj);

  NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_call_ensure_recoverable();)

  while (worklist->is_empty() == false) {
    oop cur_obj = worklist->pop();
    nvmOop cur_nvm_obj = cur_obj->nvm_header().fwd();

    NVM_COUNTER_ONLY(cur_thread->nvm_counter()->inc_persistent_obj(cur_obj->size());)

    OurPersist::add_dependent_obj_list(cur_nvm_obj, cur_thread);
    OurPersist::copy_object_thread_local(cur_obj);
  }

  // sfence
  NVM_FENCE

  OurPersist::clear_responsible_thread(cur_thread);
}

void OurPersist::copy_object_thread_local(oop obj) {
  // we do not lock or detect dependency or verify here
  // it is because obj is thread-local
  assert(obj != NULL, "obj: %p", OOP_TO_VOID(obj));
  assert(obj->nvm_header().fwd() != NULL, "fwd: %p", obj->nvm_header().fwd());
  assert(obj->nvm_header().fwd()->responsible_thread() == Thread::current(),
         "fwd: %p", obj->nvm_header().fwd());

  Klass* klass = obj->klass();
  nvmOop nvm_obj = obj->nvm_header().fwd();
  size_t obj_size = obj->size() * HeapWordSize;

  Thread* cur_thread = Thread::current();
  NVMWorkListStack* worklist = cur_thread->nvm_work_list();

  // copy step
  OurPersist::copy_object_copy_step_thread_local(obj, nvm_obj, klass, worklist, cur_thread);

  // write back & sfence
  NVM_FLUSH_LOOP(nvm_obj, obj->size() * HeapWordSize);

    // verify step
  assert(OurPersist::copy_object_verify_step(obj, nvm_obj, klass), "must pass");

}

void OurPersist::copy_object_copy_step_thread_local(oop obj, nvmOop nvm_obj, Klass* klass,
                                       NVMWorkListStack* worklist, Thread* cur_thread) {
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
          nvmOop nvm_v = NULL;

          if (v != NULL && OurPersist::is_target(v->klass())) {
            assert(!(v->nvm_header().recoverable()), "sanity check");
            nvm_v = v->nvm_header().fwd();
            if (nvm_v == nullptr) {
              bool success = OurPersist::shade(v, cur_thread);
              assert(success, "must be");
              nvm_v = v->nvm_header().fwd();
              worklist->add(v);
            }
            assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
            assert(nvm_v->responsible_thread() == Thread::current(), "fwd: %p", nvm_v);
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

    objArrayOop(nvm_obj)->set_length(array_length);

    for (int i = 0; i < array_length; i++) {
      ptrdiff_t field_offset = objArrayOopDesc::base_offset_in_bytes() + type2aelembytes(array_type) * i;
      const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP | IS_ARRAY;
      typedef CardTableBarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Parent;
      typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;
      oop v = Parent::oop_load_in_heap_at(obj, field_offset);

      nvmOop nvm_v = NULL;
      if (v != NULL && OurPersist::is_target(v->klass())) {
        assert(!(v->nvm_header().recoverable()), "sanity check");
        nvm_v = v->nvm_header().fwd();
        if (nvm_v == nullptr) {
          bool success = OurPersist::shade(v, cur_thread);
          assert(success, "must be");
          nvm_v = v->nvm_header().fwd();
          worklist->add(v);

        }
        assert(nvmHeader::is_fwd(nvm_v), "nvm_v: %p", nvm_v);
        assert(nvm_v->responsible_thread() == Thread::current(), "fwd: %p", nvm_v);

      }
      Raw::oop_store_in_heap_at((oop)nvm_obj, field_offset, (oop)nvm_v);
    }
  } else if (klass->is_typeArray_klass()) {
    TypeArrayKlass* ak = (TypeArrayKlass*)klass;
    typeArrayOop ao = (typeArrayOop)obj;
    BasicType array_type = ((ArrayKlass*)ak)->element_type();
    int array_length = ao->length();

    typeArrayOop(nvm_obj)->set_length(array_length);

    void* obj_ptr = OOP_TO_VOID(obj);
    int base_offset_in_bytes = typeArrayOopDesc::base_offset_in_bytes(array_type);
    int base_offset_in_words = base_offset_in_bytes / HeapWordSize;
    assert(base_offset_in_bytes % HeapWordSize == 0, "");
    HeapWord* to   = (HeapWord*)((char*)nvm_obj + base_offset_in_bytes);
    HeapWord* from = (HeapWord*)((char*)obj_ptr + base_offset_in_bytes);
    Copy::aligned_disjoint_words(from, to, obj->size() - base_offset_in_words /* word size */);
  } else {
    report_vm_error(__FILE__, __LINE__, "Illegal field type.");
  }
  // end "for f in obj.fields"
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
