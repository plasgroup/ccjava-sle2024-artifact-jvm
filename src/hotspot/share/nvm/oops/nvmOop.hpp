#ifdef OUR_PERSIST

#ifndef NVM_OOPS_NVMOOPDESC_HPP
#define NVM_OOPS_NVMOOPDESC_HPP

#include "memory/allStatic.hpp"
#include "oops/oopsHierarchy.hpp"
#include "utilities/globalDefinitions.hpp"

typedef class nvmMirrorOopDesc* nvmMirrorOop;
typedef class nvmOopDesc* nvmOop;
class nvmOopDesc {
 private:
  uintptr_t _responsible_thread;
#ifdef ASSERT
  uintptr_t _assert_unused;
#endif // ASSERT
  // LSB is mark bit.
  // if LSB is 0, it means this field is dependent_object_list.
  // if LSB is 1, it means this field is dram_copy.
  uintptr_t _dependent_object_next_OR_dram_copy_and_mark;
  uintptr_t _klass;

 public:

  inline Thread* responsible_thread() const {
    return (Thread*)_responsible_thread;
  }
  inline void set_responsible_thread(Thread* thread) {
    _responsible_thread = uintptr_t(thread);
  }

  inline nvmOop dependent_object_next() const {
    assert((_dependent_object_next_OR_dram_copy_and_mark & 0x1) == 0, "");
    return (nvmOop)_dependent_object_next_OR_dram_copy_and_mark;
  }
  inline void set_dependent_object_next(nvmOop obj) {
    _dependent_object_next_OR_dram_copy_and_mark = uintptr_t(obj);
  }

  inline oopDesc* dram_copy() const {
    assert(mark(), "");
    return (oopDesc*)(_dependent_object_next_OR_dram_copy_and_mark & ~0x1);
  }
  inline void set_dram_copy(oopDesc* obj) {
    assert(mark(), "");
    _dependent_object_next_OR_dram_copy_and_mark = uintptr_t(obj) | 0x1;
  }

  inline bool mark() const {
    return (_dependent_object_next_OR_dram_copy_and_mark & 0x1) != 0;
  }
  inline void set_mark() {
    assert(!mark(), "");
    _dependent_object_next_OR_dram_copy_and_mark = 0x1;
  }

  inline void clear_dram_copy_and_mark() {
    assert(mark() && (dram_copy() != NULL), "");
    _dependent_object_next_OR_dram_copy_and_mark = 0;
  }

  inline nvmMirrorOop klass() const {
    return (nvmMirrorOop)_klass;
  }
  inline void set_klass(nvmMirrorOop klass) {
    _klass = uintptr_t(klass);
  }

  inline static ptrdiff_t responsible_thread_offset() {
    return offset_of(nvmOopDesc, _responsible_thread);
  }
  inline static ptrdiff_t dependent_object_next_OR_dram_copy_offset() {
    return offset_of(nvmOopDesc, _dependent_object_next_OR_dram_copy_and_mark);
  }
  inline static ptrdiff_t klass_offset() {
    return offset_of(nvmOopDesc, _klass);
  }

  template <typename T>
  static T load_at(nvmOop base, ptrdiff_t offset) {
    return *(T*)(((address)base) + offset);
  }
  template <typename T>
  static void store_at(nvmOop base, ptrdiff_t offset, T value) {
    *(T*)(((address)base) + offset) = value;
  }
  template <typename T>
  T load_at(ptrdiff_t offset) {
    return load_at<T>(this, offset);
  }
  template <typename T>
  void store_at(ptrdiff_t offset, T value) {
    store_at(this, offset, value);
  }
};

#endif // NVM_OOPS_NVMOOPDESC_HPP

#endif // OUR_PERSIST
