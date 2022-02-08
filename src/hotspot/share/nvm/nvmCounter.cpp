#ifdef NVM_COUNTER

#include "nvm/nvmCounter.hpp"
#include "nvm/nvmDebug.hpp"
#include "runtime/thread.hpp"
#include "memory/resourceArea.hpp"

// global counters
unsigned long NVMCounter::_alloc_nvm_g = 0;
unsigned long NVMCounter::_persistent_obj_g = 0;
unsigned long NVMCounter::_conpy_obj_retry_g = 0;

// for debug
unsigned long NVMCounter::_thr_create = 0;
unsigned long NVMCounter::_thr_delete = 0;
bool NVMCounter::_enable_g = true;

// others
pthread_mutex_t NVMCounter::_mtx = PTHREAD_MUTEX_INITIALIZER;
#ifdef NVM_COUNTER_CHECK_DACAPO_RUN
bool NVMCounter::_countable = false;
#else
bool NVMCounter::_countable = true;
#endif // NVM_COUNTER_CHECK_DACAPO_START

void NVMCounter::entry() {
  _enable = true;
  _alloc_nvm = 0;
  _persistent_obj = 0;
  _conpy_obj_retry = 0;
  Atomic::inc(&_thr_create);
}

void NVMCounter::exit() {
  assert(_enable, "");
  _enable = false;

  pthread_mutex_lock(&_mtx);
  _thr_delete++;

  _alloc_nvm_g += _alloc_nvm;
  _alloc_nvm = 0;

  _persistent_obj_g += _persistent_obj;
  _persistent_obj = 0;

  _conpy_obj_retry_g += _conpy_obj_retry;
  _conpy_obj_retry = 0;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::print() {
  assert(_thr_create == _thr_delete, "");
  _enable_g = false;

  #define NVMCOUNTER_PREFIX "[NVMCounter] "
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_create:       %lu", _thr_create);
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_delete:       %lu", _thr_delete);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_nvm_g:      %lu", _alloc_nvm_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_persistent_obj_g: %lu", _persistent_obj_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_copy_obj_retry_g: %lu", _copy_obj_retry_g);
}

#endif // NVM_COUNTER
