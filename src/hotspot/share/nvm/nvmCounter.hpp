#ifdef OUR_PERSIST
#ifdef ASSERT
#ifdef NVM_COUNTER

#ifndef NVM_NVMCOUNTER_HPP
#define NVM_NVMCOUNTER_HPP

#include "memory/allocation.hpp"
#include "runtime/atomic.hpp"
#include "utilities/ostream.hpp"

// ===== 方針等 =====
// 色々な回数を計測するクラス
// スレッドローカルにカウンターを保持しておき，スレッド終了時にグローバルな変数にマージする
// VM 終了時に残っているスレッドのカウンタを強制マージして，カウント値を出力する
// ===== 注意点 =====
// 下記で説明する "カウントアップ関数" 以外の関数を勝手に呼び出してはいけない
// 関連する全ての処理は，OUR_PERSIST，ASSERT，NVM_COUNTER マクロで囲む
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
  // local counters
  unsigned long _alloc_nvm;
  unsigned long _persistent_obj;

  // global counters
  static unsigned long _alloc_nvm_g;
  static unsigned long _persistent_obj_g;

  // for debug
  bool _enable;
  static bool _enable_g;
  static unsigned long _thr_create;
  static unsigned long _thr_delete;

  // others
  static pthread_mutex_t _mtx;
  static bool _countable;

 public:
  NVMCounter() {
    entry();
  }

  ~NVMCounter() {
    if (_enable_g) {
      exit();
    }
  }

 private:
  inline static bool countable() { return _countable; };
  inline static void set_countable(bool countable) {
    _countable = countable;
  };

 public:
  inline void inc_alloc_nvm() { if (countable()) _alloc_nvm++; }
  inline void inc_persistent_obj() { if (countable()) _persistent_obj++; }
  void entry();
  void exit();
  static void print();

};

#endif // NVM_NVMCOUNTER_HPP
#endif // NVM_COUNTER
#endif // ASSERT
#endif // NVM_COUNTER
