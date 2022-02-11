#ifdef OUR_PERSIST

#ifndef NVM_NVMBARRIERSYNC_HPP
#define NVM_NVMBARRIERSYNC_HPP

#include "memory/allocation.hpp"
#include "nvm/ourPersist.hpp"

class NVMBarrierSync : public CHeapObj<mtNone> {
 private:
  NVMBarrierSync* _parent;
  volatile unsigned long _sync_count;
  volatile unsigned long _ref_count;
  static pthread_mutex_t _mtx;
#ifdef ASSERT
  static Thread* _locked_thread;
#endif // ASSERT

 public:
  NVMBarrierSync() {
    _parent = NULL;
    _sync_count = 0;
    _ref_count = 0;
  }

  ~NVMBarrierSync() {
#ifdef ASSERT
    bool parent_verify     = _parent == NULL;
    bool sync_count_verify = _sync_count == 0;
    bool ref_count_verify  = _ref_count == 0;
    assert(parent_verify && sync_count_verify && ref_count_verify,
           "_parent: %p, _sync_count: %lu, _ref_count: %lu",
           _parent, _sync_count, _ref_count);
#endif
  }

 private:
  inline static void lock();
  inline static void unlock();
#ifdef ASSERT
  inline static bool is_locked();
#endif // ASSERT

 private:
  // WARNING: Almost all non-static private functions are critical sections.
  inline NVMBarrierSync* leader();
  inline NVMBarrierSync* parent();
  inline void set_parent(NVMBarrierSync* parent);

  inline unsigned long sync_count();
  inline void move_sync_count(NVMBarrierSync* from);
  inline void dec_sync_count();

  inline unsigned long ref_count();
  inline unsigned long ref_count_nolock(); // for sync_phase2()
  inline void inc_ref_count();
  inline void dec_ref_count();

 public:
  inline void init();
  inline void add(oop obj, void* nvm_obj, Thread* cur_thread);
  // NOTE: Make the object recoverable between sync_phase1 and sync_phase2
  inline void sync_phase1(); // synchornization complete
  inline void sync_phase2(); // exit from the thread tree

};

#endif // NVM_NVMBARRIERSYNC_HPP

#endif // OUR_PERSIST
