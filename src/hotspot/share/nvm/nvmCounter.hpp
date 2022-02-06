#ifdef OUR_PERSIST
#ifdef ASSERT
#ifdef NVM_COUNTER

#ifndef NVM_NVMCOUNTER_HPP
#define NVM_NVMCOUNTER_HPP

#include "memory/allocation.hpp"
#include "runtime/atomic.hpp"
#include "utilities/ostream.hpp"

class NVMCounter: public CHeapObj<mtNone> {
 private:
  bool _enable;
  unsigned long _alloc_nvm;
  unsigned long _persistent_obj;

  static unsigned long _alloc_nvm_g;
  static unsigned long _persistent_obj_g;

  static unsigned long _thr_create;
  static unsigned long _thr_delete;

  static pthread_mutex_t _mtx;
  static bool _dacapo_benchmark_start;

 public:
  NVMCounter() {
    entry();
  }

  ~NVMCounter() {
    exit();
  }

 private:

 public:
  inline void inc_alloc_nvm() { _alloc_nvm++; }
  inline void inc_persistent_obj() { _persistent_obj++; }
  void entry();
  void exit();
  static void print();
  static bool dacapo_benchmark_start() { return _dacapo_benchmark_start; };
  static void set_dacapo_benchmark_start(bool start) {
    _dacapo_benchmark_start = start;
  };

};

#endif // NVM_NVMCOUNTER_HPP
#endif // NVM_COUNTER
#endif // ASSERT
#endif // NVM_COUNTER
