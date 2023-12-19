#ifdef OUR_PERSIST

#ifndef NVM_NVMBARRIERSYNC_INLINE_HPP
#define NVM_NVMBARRIERSYNC_INLINE_HPP

#include "nvm/nvmBarrierSync.hpp"
#include "oops/oop.inline.hpp"

inline void NVMBarrierSync::lock() {
  pthread_mutex_t* mutex = &NVMBarrierSync::_mtx;
  pthread_mutex_lock(mutex);

#ifdef ASSERT
  assert(NVMBarrierSync::_locked_thread == NULL,
         "already locked by thread %p ???",
         NVMBarrierSync::_locked_thread);
  NVMBarrierSync::_locked_thread = Thread::current();
#endif // ASSERT
}

inline void NVMBarrierSync::unlock() {
#ifdef ASSERT
  assert(NVMBarrierSync::_locked_thread == Thread::current(),
         "unlock() called by thread %p, but locked by thread %p",
         Thread::current(), NVMBarrierSync::_locked_thread);
  NVMBarrierSync::_locked_thread = NULL;
#endif // ASSERT

  pthread_mutex_t* mutex = &NVMBarrierSync::_mtx;
  pthread_mutex_unlock(mutex);
}

#ifdef ASSERT
inline bool NVMBarrierSync::is_locked() {
  pthread_mutex_t* mutex = &NVMBarrierSync::_mtx;
  int res = pthread_mutex_trylock(mutex);
  bool failure = res != 0;

  if (!failure) {
    pthread_mutex_unlock(mutex);
  } else {
    assert(res != EINVAL, "");
    assert(res == EBUSY, "");
  }

  return failure;
}
#endif // ASSERT

inline NVMBarrierSync* NVMBarrierSync::leader() {
  assert(NVMBarrierSync::is_locked(), "");
  assert(_parent != NULL, "");

  NVMBarrierSync* prev = _parent;
  NVMBarrierSync* next = prev->parent();
  while (prev != next) {
    assert(prev != NULL && next != NULL, "");
    prev = next;
    next = next->parent();
  }

  assert(prev != NULL, "");
  return prev;
}

inline bool NVMBarrierSync::is_same_group(JavaThread* thread_a, JavaThread* thread_b) {
  NVMBarrierSync *node_a = thread_a->nvm_barrier_sync();
  NVMBarrierSync *node_b = thread_b->nvm_barrier_sync();
  NVMBarrierSync::lock();
  bool result = node_a->_parent != nullptr && node_b->_parent != nullptr && node_a->leader() == node_b->leader();
  NVMBarrierSync::unlock();
  return result;
}

inline NVMBarrierSync* NVMBarrierSync::parent() {
  assert(NVMBarrierSync::is_locked(), "");
  return _parent;
}

inline void NVMBarrierSync::set_parent(NVMBarrierSync* parent) {
  assert(NVMBarrierSync::is_locked(), "");
  _parent = parent;
}

inline unsigned long NVMBarrierSync::sync_count() {
  assert(NVMBarrierSync::is_locked(), "");
  return _sync_count;
}

inline void NVMBarrierSync::move_sync_count(NVMBarrierSync* from) {
  assert(NVMBarrierSync::is_locked(), "");
  _sync_count += from->_sync_count;
  from->_sync_count = 0;
}

inline void NVMBarrierSync::dec_sync_count() {
  assert(NVMBarrierSync::is_locked(), "");
  assert(_sync_count != 0, "");
  _sync_count--;
}

inline unsigned long NVMBarrierSync::ref_count() {
  assert(NVMBarrierSync::is_locked(), "");
  return _ref_count;
}

inline unsigned long NVMBarrierSync::ref_count_nolock() {
  return _ref_count;
}

inline void NVMBarrierSync::inc_ref_count() {
  assert(NVMBarrierSync::is_locked(), "");
  _ref_count++;
}

inline void NVMBarrierSync::dec_ref_count() {
  assert(NVMBarrierSync::is_locked(), "");
  assert(_ref_count > 0, "");
  _ref_count--;
}

inline void NVMBarrierSync::init() {
#ifdef ASSERT
    bool parent_verify     = _parent == NULL;
    bool sync_count_verify = _sync_count == 0;
    bool ref_count_verify  = _ref_count == 0;
    assert(parent_verify && sync_count_verify && ref_count_verify,
           "_parent: %p, _sync_count: %lu, _ref_count: %lu",
           _parent, _sync_count, _ref_count);
#endif
  _parent = this;
  _sync_count = 1;
  _ref_count = 0;
}

inline void NVMBarrierSync::add(oop obj, nvmOop nvm_obj, Thread* cur_thread) {
  assert(obj != NULL, "");
  assert(obj->nvm_header().fwd() == nvm_obj, "");
  assert(nvmHeader::is_fwd(nvm_obj), "");

  Thread* dependant_thread = nvm_obj->responsible_thread();
  if (dependant_thread == NULL || dependant_thread == cur_thread) {
    return;
  }

  NVMBarrierSync* node = dependant_thread->nvm_barrier_sync();
  assert(node != NULL, "");

  NVMBarrierSync::lock();

  if (node->parent() == NULL) {
    NVMBarrierSync::unlock();
    return;
  }

  assert(_parent != NULL, "");
  NVMBarrierSync* cur_leader = this->leader();
  NVMBarrierSync* new_leader = node->leader();
  unsigned long cur_sync_count = cur_leader->sync_count();
  unsigned long new_sync_count = new_leader->sync_count();

  if (new_sync_count == 0) {
    NVMBarrierSync::unlock();
    return;
  }
  if (cur_leader == new_leader) {
    NVMBarrierSync::unlock();
    return;
  }

  new_leader->move_sync_count(cur_leader);
  new_leader->inc_ref_count();
  cur_leader->set_parent(new_leader);

  // TODO: option
  // update parent
  // this->set_parent(prev);
  // TODO: update parent's ref_count

  NVMBarrierSync::unlock();
}

inline void NVMBarrierSync::sync() {
  unsigned long sync_count = 0;
  NVMBarrierSync* leader  = NULL;

  // decrement _sync_count and see if this thread needs to wait
  // for other threads.
  NVMBarrierSync::lock();
  leader = this->leader();
  leader->dec_sync_count();
  sync_count = leader->sync_count();
  NVMBarrierSync::unlock();

  // wait for other threads
  while (sync_count != 0) {
    NVMBarrierSync::lock();
    leader = this->leader();
    sync_count = leader->sync_count();
    NVMBarrierSync::unlock();
  }

  // synchornization complete

  // wait for children threads
  while (this->ref_count_nolock() != 0) {
    // busy wait
  }

  // exit from the thread tree
  NVMBarrierSync::lock();
  assert(this->ref_count() == 0, "");
  NVMBarrierSync* parent = this->parent();
  if (parent != this) {
    parent->dec_ref_count();
  }
  this->set_parent(NULL);
  NVMBarrierSync::unlock();
}

#endif // NVM_NVMBARRIERSYNC_INLINE_HPP

#endif // OUR_PERSIST
