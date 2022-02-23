#ifdef NVM_COUNTER

#include "nvm/nvmCounter.hpp"
#include "nvm/nvmDebug.hpp"
#include "runtime/thread.hpp"
#include "memory/resourceArea.hpp"

// global counters
unsigned long NVMCounter::_alloc_nvm_g = 0;
unsigned long NVMCounter::_persistent_obj_g = 0;
unsigned long NVMCounter::_copy_obj_retry_g = 0;

// for debug
unsigned long NVMCounter::_thr_create = 0;
unsigned long NVMCounter::_thr_delete = 0;
bool NVMCounter::_enable_g = true;
#ifdef ASSERT
Thread* NVMCounter::_thr = NULL;
Thread* NVMCounter::_thr_list[_thr_list_size] = {NULL};
#endif // ASSERT

// others
pthread_mutex_t NVMCounter::_mtx = PTHREAD_MUTEX_INITIALIZER;
#ifdef NVM_COUNTER_CHECK_DACAPO_RUN
bool NVMCounter::_countable = false;
#else
bool NVMCounter::_countable = true;
#endif // NVM_COUNTER_CHECK_DACAPO_START

void NVMCounter::entry(DEBUG_ONLY(Thread* cur_thread)) {
  _enable = true;
  _alloc_nvm = 0;
  _persistent_obj = 0;
  _copy_obj_retry = 0;

  pthread_mutex_lock(&_mtx);
#ifdef ASSERT
  assert(cur_thread != NULL, "cur_thread is NULL.");
  assert(_thr_create < _thr_list_size, "too many threads.");

  _thr = cur_thread;
  _thr_list[_thr_create] = cur_thread;
  assert(cur_thread == _thr, "");
#endif // ASSERT

  _thr_create++;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::exit(DEBUG_ONLY(Thread* cur_thread)) {
  assert(_enable, "");
  _enable = false;

  pthread_mutex_lock(&_mtx);
  _thr_delete++;

#ifdef ASSERT
  assert(cur_thread != NULL, "cur_thread is NULL.");
  assert(cur_thread == _thr, "");
  bool found = false;
  for (unsigned long i = 0; i < _thr_create; i++) {
    if (_thr_list[i] != cur_thread) continue;
    _thr_list[i] = NULL; found = true; break;
  }
  assert(!found, "cur_thread is not found. Thread name: %s", cur_thread->name());
#endif // ASSERT

  _alloc_nvm_g += _alloc_nvm;
  _alloc_nvm = 0;

  _persistent_obj_g += _persistent_obj;
  _persistent_obj = 0;

  _copy_obj_retry_g += _copy_obj_retry;
  _copy_obj_retry = 0;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::print() {
#ifdef ASSERT
  for (unsigned long i = 0; i < _thr_create; i++) {
    if (_thr_list[i] == NULL) continue;
    assert(_thr_create != _thr_delete, "");
    tty->print_cr("exit() has not been executed. Thread name: %s", _thr_list[i]->name());
  }
  assert(_thr_create == _thr_delete, "");
#endif // ASSERT
  _enable_g = false;

  #define NVMCOUNTER_PREFIX "[NVMCounter] "
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_create:       %lu", _thr_create);
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_delete:       %lu", _thr_delete);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_nvm_g:      %lu", _alloc_nvm_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_persistent_obj_g: %lu", _persistent_obj_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_copy_obj_retry_g: %lu", _copy_obj_retry_g);
}

#endif // NVM_COUNTER
