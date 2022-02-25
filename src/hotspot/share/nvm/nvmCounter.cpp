#ifdef NVM_COUNTER

#include "memory/resourceArea.hpp"
#include "nvm/nvmCounter.hpp"
#include "nvm/nvmDebug.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.inline.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/fieldDescriptor.inline.hpp"
#include "runtime/thread.hpp"

// global counters
unsigned long NVMCounter::_alloc_nvm_g = 0;
unsigned long NVMCounter::_persistent_obj_g = 0;
unsigned long NVMCounter::_copy_obj_retry_g = 0;
unsigned long NVMCounter::_access_g[NVMCounter::_access_n] = {0};
unsigned long NVMCounter::_fields_g = 0;
unsigned long NVMCounter::_volatile_fields_g = 0;

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
  for (int i = 0; i < _access_n; i++) {
    _access[i] = 0;
  }
  _fields = 0;
  _volatile_fields = 0;

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
  // FIXME:
  // assert(cur_thread == _thr, "");
  bool found = false;
  for (unsigned long i = 0; i < _thr_create; i++) {
    if (_thr_list[i] != cur_thread) continue;
    _thr_list[i] = NULL; found = true; break;
  }
  // FIXME:
  // assert(!found, "cur_thread is not found. Thread name: %s", cur_thread->name());
#endif // ASSERT

  _alloc_nvm_g += _alloc_nvm;
  _alloc_nvm = 0;

  _persistent_obj_g += _persistent_obj;
  _persistent_obj = 0;

  _copy_obj_retry_g += _copy_obj_retry;
  _copy_obj_retry = 0;

  for (int i = 0; i < _access_n; i++) {
    _access_g[i] += _access[i];
    _access[i] = 0;
  }

  _fields_g += _fields;
  _fields = 0;

  _volatile_fields_g += _volatile_fields;
  _volatile_fields = 0;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::inc_access(bool is_store, oop obj, ptrdiff_t offset) {
  if (!is_store) {
    // TODO:
    return;
    if (!countable()) {
      return;
    }
    Klass* k = obj->klass();
    if (k->is_instance_klass()) {
      return;
    }
    if (k->is_typeArray_klass()) {
      return;
    }
    if (k->is_objArray_klass()) {
      return;
    }
    report_vm_error(__FILE__, __LINE__, "should not reach here.");
    return;
    //tty->print_cr("should not reach here.");
    if (!SystemDictionary::Class_klass_loaded()) {
      //tty->print_cr("counter return. countable: %d", _countable);
      return;
    }
  }

  Klass* k = obj->klass();
  bool is_static   = k != NULL &&
                     k->id() == KlassID::InstanceMirrorKlassID &&
                     offset < InstanceMirrorKlass::offset_of_static_fields();
  bool is_volatile = false;
  bool is_oop      = false;
  bool is_runtime  = true;

  if (k->is_instance_klass()) {
    fieldDescriptor fd;
    ((InstanceKlass*)k)->find_field_from_offset(offset, is_static, &fd);
    is_volatile = fd.is_volatile();
    is_oop = is_reference_type(fd.field_type());
  } else {
    is_oop = k->is_objArray_klass();
  }

  inc_access(is_store, is_volatile, is_oop, is_static, is_runtime);
}

unsigned long NVMCounter::get_access(int is_store, int is_volatile, int is_oop, int is_static, int is_runtime) {
  // 1: true, 0: false, -1: ignore
  if (is_store == -1) {
    return get_access(0, is_volatile, is_oop, is_static, is_runtime) +
           get_access(1, is_volatile, is_oop, is_static, is_runtime);
  }
  if (is_volatile == -1) {
    return get_access(is_store, 0, is_oop, is_static, is_runtime) +
           get_access(is_store, 1, is_oop, is_static, is_runtime);
  }
  if (is_oop == -1) {
    return get_access(is_store, is_volatile, 0, is_static, is_runtime) +
           get_access(is_store, is_volatile, 1, is_static, is_runtime);
  }
  if (is_static == -1) {
    return get_access(is_store, is_volatile, is_oop, 0, is_runtime) +
           get_access(is_store, is_volatile, is_oop, 1, is_runtime);
  }
  if (is_runtime == -1) {
    return get_access(is_store, is_volatile, is_oop, is_static, 0) +
           get_access(is_store, is_volatile, is_oop, is_static, 1);
  }
  int flags = access_bool2flags(is_store == 1, is_volatile == 1, is_oop == 1,
                                is_static == 1, is_runtime == 1);
  return _access_g[flags];
}

void NVMCounter::print() {
#ifdef ASSERT
  for (unsigned long i = 0; i < _thr_create; i++) {
    if (_thr_list[i] == NULL) continue;
    assert(_thr_create != _thr_delete, "");
    tty->print_cr("exit() has not been executed. Thread name: %s", _thr_list[i]->name());
  }
  // FIXME:
  assert(_thr_create >= _thr_delete, "");
#endif // ASSERT
  _enable_g = false;
  if (_thr_create != _thr_delete) {
    report_vm_error(__FILE__, __LINE__, "exit() has not been executed in some threads.");
  }

  #define NVMCOUNTER_PREFIX "[NVMCounter] "
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_create:       %lu", _thr_create);
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_delete:       %lu", _thr_delete);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_nvm_g:      %lu", _alloc_nvm_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_persistent_obj_g: %lu", _persistent_obj_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_copy_obj_retry_g: %lu", _copy_obj_retry_g);

  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (is_store, is_volatile, is_oop, is_static, is_runtime)");
  for (int i = 0; i < _access_n; i++) {
    bool is_store    = i & 0b00001;
    bool is_volatile = i & 0b00010;
    bool is_oop      = i & 0b00100;
    bool is_static   = i & 0b01000;
    bool is_runtime  = i & 0b10000;
    assert(NVMCounter::access_bool2flags(is_store, is_volatile, is_oop, is_static, is_runtime) == i, "");
    tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (%d, %d, %d, %d, %d) %lu",
                  is_store, is_volatile, is_oop, is_static, is_runtime, _access_g[i]);
  }
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (store) %lu", get_access(1, -1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (load)  %lu", get_access(0, -1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (store/volatile) %lu", get_access(1, 1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (load/volatile)  %lu", get_access(0, 1, -1, -1, -1));

  tty->print_cr(NVMCOUNTER_PREFIX "_fields_g:          %lu", _fields_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_volatile_fields_g: %lu", _volatile_fields_g);
}

#endif // NVM_COUNTER
