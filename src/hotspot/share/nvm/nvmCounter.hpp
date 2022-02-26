#ifdef NVM_COUNTER

#ifndef NVM_NVMCOUNTER_HPP
#define NVM_NVMCOUNTER_HPP

#include "memory/allocation.hpp"
#include "runtime/atomic.hpp"
#include "utilities/ostream.hpp"
#include "interpreter/interp_masm.hpp"
class Klass;
class Method;
class MacroAssembler;

// ===== 方針等 =====
// 色々な回数を計測するクラス
// スレッドローカルにカウンターを保持しておき，スレッド終了時にグローバルな変数にマージする
// VM 終了時に残っているスレッドのカウンタを強制マージして，カウント値を出力する
// ===== 注意点 =====
// 下記で説明する "カウントアップ関数" 以外の関数を勝手に呼び出してはいけない
// 関連する全ての処理は，NVM_COUNTER マクロで囲む
// ===== 使い方 =====
// 1. local counters でカウンター（メンバ変数）を定義（例: _alloc_nvm 変数）
// 2. global counters で末尾に _g を付けてカウンター（static変数）を定義（例: _alloc_nvm_g 変数）
// 3. entry() 関数にローカルカウンターの初期化処理を追加
// 4. nvmCounter.cpp 冒頭にグローバルカウンターの初期化処理を追加
// 5. exit() 関数のロックの中に，ローカルカウンターの値をグローバルカウンターの値に移す処理を追加
// 6. カウントアップ関数を定義（例: inc_alloc_nvm() 関数）
// 7. カウントアップしたい箇所から Thread::current()->nvm_counter()->カウントアップ関数(); を呼び出す

class NVMCounter: public CHeapObj<mtNone> {
 private:
  static const int _access_n = 1 << 5;

  // local counters
  unsigned long _alloc_nvm;
  unsigned long _persistent_obj;
  unsigned long _copy_obj_retry;
  unsigned long _access[_access_n];
  unsigned long _fields;
  unsigned long _volatile_fields;

  // global counters
  static unsigned long _alloc_nvm_g;
  static unsigned long _persistent_obj_g;
  static unsigned long _copy_obj_retry_g;
  static unsigned long _access_g[_access_n];
  static unsigned long _fields_g;
  static unsigned long _volatile_fields_g;

  // for debug
  bool _enable;
  static bool _enable_g;
  static unsigned long _thr_create;
  static unsigned long _thr_delete;
#ifdef ASSERT
  static Thread* _thr;
  static const unsigned long _thr_list_size = 512;
  static Thread* _thr_list[_thr_list_size];
#endif // ASSERT

  // others
  static pthread_mutex_t _mtx;
  static bool _countable;

 public:
  NVMCounter(DEBUG_ONLY(Thread* cur_thread)) {
    entry(DEBUG_ONLY(cur_thread));
  }

  ~NVMCounter() {
    assert(!_enable, "");
  }

 private:
  inline static bool countable() { return _countable; };
  inline static void set_countable(bool countable) {
    _countable = countable;
  };

 public:
  inline void add_count(unsigned long* count, unsigned long value) {
    if (countable()) return;
    assert((~0UL) - (*count) >= value, "overflow.");
    *count += value;
  }

  inline void inc_alloc_nvm()       { add_count(&_alloc_nvm, 1); }
  inline void inc_persistent_obj()  { add_count(&_persistent_obj, 1); }
  inline void inc_copy_obj_retry()  { add_count(&_copy_obj_retry, 1); }
  inline void inc_access(int flags) { add_count(&_access[flags], 1); }
  inline void inc_access(bool is_store, bool is_volatile, bool is_oop,
                         bool is_static, bool is_runtime) {
    inc_access(access_bool2flags(is_store, is_volatile, is_oop, is_static, is_runtime));
  }
  void inc_access(bool is_store, oop obj, ptrdiff_t offset, bool is_volatile, bool is_oop);
  inline void inc_fields()          { add_count(&_fields, 1); }
  inline void inc_volatile_fields() { add_count(&_volatile_fields, 1); }

  static unsigned long get_access(int is_store, int is_volatile, int is_oop, int is_static, int is_runtime);
  inline static int access_bool2flags(bool is_store, bool is_volatile, bool is_oop,
                                      bool is_static, bool is_runtime) {
    int flags = 0;
    if (is_store)    flags |= 0b00001;
    if (is_volatile) flags |= 0b00010;
    if (is_oop)      flags |= 0b00100;
    if (is_static)   flags |= 0b01000;
    if (is_runtime)  flags |= 0b10000;
    return flags;
  }

  // for assembller
  static void inc_access(Thread* thr, int flags);
  static void inc_access_asm(MacroAssembler* masm, bool is_store, bool is_volatile, bool is_oop,
                             bool is_static, bool is_runtime);

  void entry(DEBUG_ONLY(Thread* cur_thread));
  void exit(DEBUG_ONLY(Thread* cur_thread));
  static void print();

#ifdef NVM_COUNTER_CHECK_DACAPO_RUN
  static void set_dacapo_countable_flag(Klass* k, Method* m);
  static void set_dacapo_countable_flag_asm(MacroAssembler* masm, Register method,
                                            Register index, Register recv);
#endif // NVM_COUNTER_CHECK_DACAPO_RUN

};

#endif // NVM_NVMCOUNTER_HPP
#endif // NVM_COUNTER
