#ifdef OUR_PERSIST
#ifdef ASSERT
#ifdef NVM_COUNTER

#include "nvm/nvmCounter.hpp"
#include "nvm/nvmDebug.hpp"
#include "runtime/thread.hpp"
#include "memory/resourceArea.hpp"

pthread_mutex_t NVMCounter::_mtx = PTHREAD_MUTEX_INITIALIZER;
unsigned long NVMCounter::_alloc_nvm_g = 0;
unsigned long NVMCounter::_persistent_obj_g = 0;
unsigned long NVMCounter::_thr_create = 0;
unsigned long NVMCounter::_thr_delete = 0;
bool NVMCounter::_dacapo_benchmark_start = false;

void NVMCounter::entry() {
  _enable = true;
  _alloc_nvm = 0;
  _persistent_obj = 0;
  Atomic::inc(&_thr_create);
}

void NVMCounter::exit() {
  if (!_enable) return;

  _enable = false;
  pthread_mutex_lock(&_mtx);
  _alloc_nvm_g += _alloc_nvm;
  _alloc_nvm = 0;
  _persistent_obj_g += _persistent_obj;
  _persistent_obj = 0;
  _thr_delete++;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::print() {
  tty->print_cr("[NVMCounter] _alloc_nvm_g: %lu", _alloc_nvm_g);
  tty->print_cr("[NVMCounter] _persistent_obj_g: %lu", _persistent_obj_g);
  tty->print_cr("[NVMCounter] _thr_create: %lu", _thr_create);
  tty->print_cr("[NVMCounter] _thr_delete: %lu", _thr_delete);
}

#endif // NVM_COUNTER
#endif // ASSERT
#endif // NVM_COUNTER
