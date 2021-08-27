#ifdef OUR_PERSIST

#ifndef NVM_BARRIERSYNC_HPP
#define NVM_BARRIERSYNC_HPP

#include "memory/allocation.hpp"
#include "nvm/ourPersist.hpp"

class NVMBarrierSync : public CHeapObj<mtNone>{
 private:
  NVMBarrierSync* _parent;
  unsigned long _sync_count;
  unsigned long _ref_count;
  static pthread_mutex_t _mtx;

 public:
  NVMBarrierSync() {
    _parent = NULL;
    _sync_count = 0;
    _ref_count = 0;
  }

  ~NVMBarrierSync() {
    assert(_parent == NULL, "");
    assert(_sync_count == 0, "");
    // NOTE: No one loads after the count reach zero.
    assert(_ref_count == 0 || _ref_count == 18446744073709551615UL /* -1 */, "");
  }

 private:
  // WARNING: Almost all private functions are critical sections.
  inline NVMBarrierSync* leader();
  inline NVMBarrierSync* parent();
  inline void set_parent(NVMBarrierSync* parent);

  inline unsigned long sync_count();
  inline unsigned long atomic_sync_count();
  inline void add_sync_count(unsigned long val);
  inline void dec_sync_count();

  inline unsigned long ref_count();
  inline unsigned long atomic_ref_count();
  inline void inc_ref_count();
  inline void dec_ref_count();
  inline void atomic_dec_ref_count();

 public:
  inline void init();
  inline void add(NVMBarrierSync* node, oop obj);
  inline void sync();

};

#endif // NVM_BARRIERSYNC_HPP

#endif // OUR_PERSIST
