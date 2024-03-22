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
unsigned long NVMCounter::_alloc_dram_g = 0;
unsigned long NVMCounter::_alloc_dram_word_g = 0;
unsigned long NVMCounter::_alloc_nvm_g = 0;
unsigned long NVMCounter::_alloc_nvm_word_g = 0;
unsigned long NVMCounter::_persistent_obj_g = 0;
unsigned long NVMCounter::_persistent_obj_word_g = 0;
unsigned long NVMCounter::_copy_obj_retry_g = 0;
unsigned long NVMCounter::_access_g[NVMCounter::_access_n] = {0};
unsigned long NVMCounter::_fields_g = 0;
unsigned long NVMCounter::_volatile_fields_g = 0;
unsigned long NVMCounter::_clwb_g = 0;
unsigned long NVMCounter::_call_ensure_recoverable_g = 0;

// for debug
unsigned long NVMCounter::_thr_create = 0;
unsigned long NVMCounter::_thr_delete = 0;
bool NVMCounter::_enable_g = true;
#ifdef ASSERT
Thread* NVMCounter::_thr_list[_thr_list_size] = {NULL};
#endif // ASSERT
NVMCounter* NVMCounter::_cnt_list[_cnt_list_size] = {NULL};

// others
pthread_mutex_t NVMCounter::_mtx = PTHREAD_MUTEX_INITIALIZER;
#ifdef NVM_COUNTER_CHECK_DACAPO_RUN
bool NVMCounter::_countable = false;
#else  // NVM_COUNTER_CHECK_DACAPO_RUN
bool NVMCounter::_countable = true;
#endif // NVM_COUNTER_CHECK_DACAPO_RUN

void NVMCounter::entry(DEBUG_ONLY(Thread* cur_thread)) {
  _enable = true;
  _alloc_dram = 0;
  _alloc_dram_word = 0;
  _alloc_nvm = 0;
  _alloc_nvm_word = 0;
  _persistent_obj = 0;
  _persistent_obj_word = 0;
  _copy_obj_retry = 0;
  for (int i = 0; i < _access_n; i++) {
    _access[i] = 0;
  }
  _fields = 0;
  _volatile_fields = 0;
  _clwb = 0;
  _call_ensure_recoverable = 0;

  pthread_mutex_lock(&_mtx);
#ifdef ASSERT
  assert(cur_thread != NULL, "cur_thread is NULL.");
  assert(_thr_create < _thr_list_size, "too many threads.");

  _thr_list_offset = _thr_create;
  _thr_list[_thr_create] = cur_thread;
#endif // ASSERT

  assert(_thr_create < _cnt_list_size, "too many counters.");
  _cnt_list_offset = _thr_create;
  _cnt_list[_thr_create] = this;

  _thr_create++;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::exit(DEBUG_ONLY(Thread* cur_thread)) {
  pthread_mutex_lock(&_mtx);

  if (!_enable) {
    pthread_mutex_unlock(&_mtx);
    return;
  }

  _enable = false;
  _thr_delete++;

#ifdef ASSERT
  assert(cur_thread != NULL, "cur_thread is NULL.");
  assert(_thr_list_offset < _thr_list_size, "out of range.");
  assert(cur_thread == _thr_list[_thr_list_offset], "");
  _thr_list[_thr_list_offset] = NULL;
  _thr_list_offset = _thr_list_size;
#endif // ASSERT

  assert(_cnt_list_offset < _thr_create, "out of range.");
  assert(_cnt_list[_cnt_list_offset] == this, "");
  _cnt_list[_cnt_list_offset] = NULL;
  _cnt_list_offset = _cnt_list_size;

  _alloc_dram_g += _alloc_dram;
  _alloc_dram = 0;

  _alloc_dram_word_g += _alloc_dram_word;
  _alloc_dram_word = 0;

  _alloc_nvm_g += _alloc_nvm;
  _alloc_nvm = 0;

  _alloc_nvm_word_g += _alloc_nvm_word;
  _alloc_nvm_word = 0;

  _persistent_obj_g += _persistent_obj;
  _persistent_obj = 0;

  _persistent_obj_word_g += _persistent_obj_word;
  _persistent_obj_word = 0;

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

  _clwb_g += _clwb;
  _clwb = 0;

  _call_ensure_recoverable_g += _call_ensure_recoverable;
  _call_ensure_recoverable = 0;
  pthread_mutex_unlock(&_mtx);
}

void NVMCounter::inc_access_runtime(int _is_store, oop obj, ptrdiff_t offset, int _is_volatile,
                                    int _is_oop, int _is_atomic) {
  assert(_is_store    ==  0 || _is_store    == 1, "");
  assert(_is_volatile == -1 || _is_volatile == 0 || _is_volatile == 1, "");
  assert(_is_oop      ==  0 || _is_oop      == 1, "");
  assert(_is_atomic   ==  0 || _is_atomic   == 1, "");

  if (_is_store == 0) {
    return; // TODO: implement
  }

  if (!countable()) {
    return;
  }

  Klass* k = obj->klass();
  assert(k != NULL, "klass is NULL.");

  bool is_store    = _is_store == 1;
  bool is_volatile = _is_volatile == 1;
  bool is_oop      = _is_oop == 1;
  bool is_static   = k->id() == KlassID::InstanceMirrorKlassID &&
                     offset < InstanceMirrorKlass::offset_of_static_fields();
  bool is_runtime  = true;
  bool is_atomic   = _is_atomic == 1;

  if (_is_volatile == -1) {
    if (k->is_instance_klass()) {
      fieldDescriptor fd;
      ((InstanceKlass*)k)->find_field_from_offset(offset, is_static, &fd);
      is_volatile = fd.is_volatile();
    } else {
      is_volatile = false;
    }
  }

  inc_access(is_store, is_volatile, is_oop, is_static, is_runtime, is_atomic);
}

unsigned long NVMCounter::get_access(int is_store, int is_volatile, int is_oop, int is_static,
                                     int is_runtime, int is_atomic) {
  // 1: true, 0: false, -1: ignore
  if (is_store == -1) {
    return get_access(0, is_volatile, is_oop, is_static, is_runtime, is_atomic) +
           get_access(1, is_volatile, is_oop, is_static, is_runtime, is_atomic);
  }
  if (is_volatile == -1) {
    return get_access(is_store, 0, is_oop, is_static, is_runtime, is_atomic) +
           get_access(is_store, 1, is_oop, is_static, is_runtime, is_atomic);
  }
  if (is_oop == -1) {
    return get_access(is_store, is_volatile, 0, is_static, is_runtime, is_atomic) +
           get_access(is_store, is_volatile, 1, is_static, is_runtime, is_atomic);
  }
  if (is_static == -1) {
    return get_access(is_store, is_volatile, is_oop, 0, is_runtime, is_atomic) +
           get_access(is_store, is_volatile, is_oop, 1, is_runtime, is_atomic);
  }
  if (is_runtime == -1) {
    return get_access(is_store, is_volatile, is_oop, is_static, 0, is_atomic) +
           get_access(is_store, is_volatile, is_oop, is_static, 1, is_atomic);
  }
  if (is_atomic == -1) {
    return get_access(is_store, is_volatile, is_oop, is_static, is_runtime, 0) +
           get_access(is_store, is_volatile, is_oop, is_static, is_runtime, 1);
  }
  int flags = access_bool2flags(is_store == 1, is_volatile == 1, is_oop == 1,
                                is_static == 1, is_runtime == 1, is_atomic == 1);
  return _access_g[flags];
}

void NVMCounter::print() {
  for (unsigned long i = 0; i < _thr_create; i++) {
    if (_cnt_list[i] == NULL) continue;
    assert(_thr_create != _thr_delete, "");
    assert(_cnt_list[i]->_enable, "");
    assert(_cnt_list[i]->_thr_list_offset == i, "");
    _cnt_list[i]->exit(DEBUG_ONLY(_thr_list[i]));
  }

#ifdef ASSERT
  for (unsigned long i = 0; i < _thr_create; i++) {
    assert(_thr_list[i] == NULL, "");
    assert(_cnt_list[i] == NULL, "");
  }
  assert(_thr_create == _thr_delete, "");
#endif // ASSERT

  _enable_g = false;
  if (_thr_create != _thr_delete) {
    report_vm_error(__FILE__, __LINE__, "exit() has not been executed in some threads.");
  }

  tty->print_cr(NVMCOUNTER_PREFIX "_thr_create:            %lu", _thr_create);
  tty->print_cr(NVMCOUNTER_PREFIX "_thr_delete:            %lu", _thr_delete);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_dram_g:          %lu", _alloc_dram_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_dram_word_g:     %lu", _alloc_dram_word_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_nvm_g:           %lu", _alloc_nvm_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_alloc_nvm_word_g:      %lu", _alloc_nvm_word_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_persistent_obj_g:      %lu", _persistent_obj_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_persistent_obj_word_g: %lu", _persistent_obj_word_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_copy_obj_retry_g:      %lu", _copy_obj_retry_g);

  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (is_store, is_volatile, is_oop, is_static, is_runtime, is_atomic)");
  for (int i = 0; i < _access_n; i++) {
    bool is_store    = i & 0b000001; // R, W
    bool is_volatile = i & 0b000010; // v
    bool is_oop      = i & 0b000100; // o, p
    bool is_static   = i & 0b001000; // s
    bool is_runtime  = i & 0b010000; // r, i
    bool is_atomic   = i & 0b100000; // a
    assert(NVMCounter::access_bool2flags(is_store, is_volatile, is_oop, is_static, is_runtime, is_atomic) == i, "");

    // FIXME: implement
    if (!is_store) continue;

    tty->print_cr(NVMCOUNTER_PREFIX "_access_g_%d%d%d%d%d%d_%s%s%s%s%s%s: %lu",
                  is_store, is_volatile, is_oop, is_static, is_runtime, is_atomic,
                  is_store ? "W" : "R", is_volatile ? "v" : "_", is_oop ? "o" : "p",
                  is_static ? "s" : "_", is_runtime ? "r" : "i", is_atomic ? "a" : "_",
                  _access_g[i]);
  }
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (store) %lu", get_access(1, -1, -1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (load)  %lu", get_access(0, -1, -1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (store/volatile) %lu", get_access(1, 1, -1, -1, -1, -1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g: (load/volatile)  %lu", get_access(0, 1, -1, -1, -1, -1));

  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (normal/non-volatile/pri) %lu", get_access(1, 0, 0, -1, -1, 0));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (normal/volatile/pri)     %lu", get_access(1, 1, 0, -1, -1, 0));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (normal/non-volatile/oop) %lu", get_access(1, 0, 1, -1, -1, 0));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (normal/volatile/oop)     %lu", get_access(1, 1, 1, -1, -1, 0));

  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (atomic/non-volatile/pri) %lu", get_access(1, 0, 0, -1, -1, 1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (atomic/volatile/pri)     %lu", get_access(1, 1, 0, -1, -1, 1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (atomic/non-volatile/oop) %lu", get_access(1, 0, 1, -1, -1, 1));
  tty->print_cr(NVMCOUNTER_PREFIX "_access_g_ismm: (atomic/volatile/oop)     %lu", get_access(1, 1, 1, -1, -1, 1));

  tty->print_cr(NVMCOUNTER_PREFIX "_fields_g:          %lu", _fields_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_volatile_fields_g: %lu", _volatile_fields_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_clwb_g:            %lu", _clwb_g);
  tty->print_cr(NVMCOUNTER_PREFIX "_call_ensure_recoverable_g: %lu", _call_ensure_recoverable_g);
  tty->print_cr(NVMCOUNTER_PREFIX "handshake count: %u", _n_handshake.load());
  tty->print_cr(NVMCOUNTER_PREFIX "full barrier count: %u", _n_full_barrier.load());
  tty->print_cr(NVMCOUNTER_PREFIX "half barrier count: %u", _n_half_barrier.load());
  tty->print_cr(NVMCOUNTER_PREFIX "no barrier count: %u", _n_no_barrier.load());
  tty->print_cr(NVMCOUNTER_PREFIX "barrier reduction: %f", static_cast<double>(_n_no_barrier.load()) / static_cast<double>(_n_no_barrier.load() + _n_half_barrier.load() + _n_full_barrier.load()));
  tty->print_cr(NVMCOUNTER_PREFIX "verification fail: %d\n", _n_fail.load());
}

class CountObjectSnapshotDuringGC : public ObjectClosure {
 private:
  unsigned long alloc_dram;
  unsigned long alloc_dram_word;
  unsigned long alloc_nvm;
  unsigned long alloc_nvm_word;

 public:
  CountObjectSnapshotDuringGC():
    alloc_dram(0), alloc_dram_word(0), alloc_nvm(0), alloc_nvm_word(0) {}

  void print() {
    tty->print_cr(NVMCOUNTER_PREFIX "(CountObjectSnapshotDuringGC)"
                  " alloc_dram: %lu, alloc_dram_word: %lu, alloc_nvm: %lu, alloc_nvm_word: %lu",
                  alloc_dram, alloc_dram_word, alloc_nvm, alloc_nvm_word);
  }

  void do_object(oop obj) {
    const int word_size = obj->size();

    if (!obj->is_gc_marked()) return;
    alloc_dram++;
    alloc_dram_word += word_size;

#ifdef OUR_PERSIST
    if (obj->nvm_header().fwd() == NULL) return;
    alloc_nvm++;
    alloc_nvm_word += word_size;
#endif // OUR_PERSIST
  }
};

void NVMCounter::count_object_snapshot_during_gc() {
  CountObjectSnapshotDuringGC closure;
  Universe::heap()->object_iterate(&closure);
  closure.print();
}

#endif // NVM_COUNTER
