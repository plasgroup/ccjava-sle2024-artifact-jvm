#ifdef OUR_PERSIST

#ifndef NVM_NVMBARRIERSYNC_INLINE_HPP
#define NVM_NVMBARRIERSYNC_INLINE_HPP

#include "nvm/nvmBarrierSync.hpp"
#include "oops/oop.inline.hpp"

inline NVMBarrierSync* NVMBarrierSync::leader() {
  assert(_parent != NULL, "");

  NVMBarrierSync* prev = _parent;
  NVMBarrierSync* next = prev->parent();
  while (prev != next) {
    prev = next;
    next = next->parent();
  }

  assert(prev != NULL, "");
  return prev;
}

inline NVMBarrierSync* NVMBarrierSync::parent() {
  return _parent;
}

inline void NVMBarrierSync::set_parent(NVMBarrierSync* parent) {
  _parent = parent;
}

inline unsigned long NVMBarrierSync::sync_count() {
  return _sync_count;
}

inline unsigned long NVMBarrierSync::atomic_sync_count() {
  return Atomic::load(&_sync_count);
}

inline void NVMBarrierSync::move_sync_count(NVMBarrierSync* from) {
  _sync_count += from->_sync_count;
  from->_sync_count = 0;
}

inline void NVMBarrierSync::dec_sync_count() {
  assert(_sync_count != 0, "");
  _sync_count--;
}

inline unsigned long NVMBarrierSync::ref_count() {
  return _ref_count;
}

inline unsigned long NVMBarrierSync::atomic_ref_count() {
  return Atomic::load(&_ref_count);
}

inline void NVMBarrierSync::inc_ref_count() {
  _ref_count++;
}

inline void NVMBarrierSync::dec_ref_count() {
  assert(_ref_count != 0, "");
  _ref_count--;
}

inline void NVMBarrierSync::atomic_dec_ref_count() {
  // NOTE: No one loads after the count reach zero.
  assert(_ref_count != 18446744073709551615UL /* -1 */, "");
  Atomic::dec(&_ref_count);
}

inline void NVMBarrierSync::init() {
#ifdef ASSERT
    bool parent_verify     = _parent == NULL;
    bool sync_count_verify = _sync_count == 0;
    // NOTE: No one loads after the count reach zero.
    bool ref_count_verify  = _ref_count == 0 || _ref_count == 18446744073709551615UL /* -1 */;
    assert(parent_verify && sync_count_verify && ref_count_verify,
           "_parent: %p, _sync_count: %lu, _ref_count: %lu",
           _parent, _sync_count, _ref_count);
#endif
  _parent = this;
  _sync_count = 1;
  _ref_count = 0;
}

inline void NVMBarrierSync::add(NVMBarrierSync* node, oop obj) {
  if (node == NULL) {
    return;
  }

  pthread_mutex_lock(&NVMBarrierSync::_mtx);

  if (node->parent() == NULL) {
    pthread_mutex_unlock(&NVMBarrierSync::_mtx);
    return;
  }

  NVMBarrierSync* cur_leader = this->leader();
  NVMBarrierSync* new_leader = node->leader();
  unsigned long cur_sync_count = cur_leader->sync_count();
  unsigned long new_sync_count = new_leader->sync_count();

  if (new_sync_count == 0) {
    pthread_mutex_unlock(&NVMBarrierSync::_mtx);
    return;
  }
  if (cur_leader == new_leader) {
    pthread_mutex_unlock(&NVMBarrierSync::_mtx);
    return;
  }

  new_leader->move_sync_count(cur_leader);
  new_leader->inc_ref_count();
  cur_leader->set_parent(new_leader);

  // TODO: option
  // update parent
  // this->set_parent(prev);
  // TODO: update parent's ref_count

  pthread_mutex_unlock(&NVMBarrierSync::_mtx);
}

inline void NVMBarrierSync::sync() {
  unsigned long sync_count = 0;
  NVMBarrierSync* leader  = NULL;

  // decrement _sync_count and see if this thread needs to wait
  // for other threads.
  pthread_mutex_lock(&NVMBarrierSync::_mtx);
  leader = this->leader();
  leader->dec_sync_count();
  sync_count = leader->sync_count();
  pthread_mutex_unlock(&NVMBarrierSync::_mtx);

  // wait for other threads
  while (sync_count != 0) {
    pthread_mutex_lock(&NVMBarrierSync::_mtx);
    leader = this->leader();
    sync_count = leader->sync_count();
    pthread_mutex_unlock(&NVMBarrierSync::_mtx);
  }

  // synchornization complete

  // wait for children threads
  if (this->ref_count() != 0) {
    while (this->atomic_ref_count() != 0) {
      // busy wait
    }
  }
  this->parent()->atomic_dec_ref_count();
  this->set_parent(NULL);
}

#endif // NVM_NVMBARRIERSYNC_INLINE_HPP

#endif // OUR_PERSIST
