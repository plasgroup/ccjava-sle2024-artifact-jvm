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
  Thread* _responsible_thread;
  nvmOop _dependent_object_next;
  nvmMirrorOop _klass;

 public:

  inline Thread* responsible_thread() const {
    return (Thread*)_responsible_thread;
  }

  inline void set_responsible_thread(Thread* t) {
    _responsible_thread = t;
  }

  inline nvmOop dependent_object_next() const {
    return _dependent_object_next;
  }

  inline void set_dependent_object_next(nvmOop obj) {
    _dependent_object_next = obj;
  }

  inline nvmMirrorOop klass() const {
    return _klass;
  }

  inline void set_klass(nvmMirrorOop klass) {
    _klass = klass;
  }

  template <typename T>
  static T load_at(nvmOop base, ptrdiff_t offset) {
    return *(T*)(((address)base) + offset);
  }

  template <typename T>
  static void store_at(nvmOop base, ptrdiff_t offset, T value) {
    *(T*)(((address)base) + offset) = value;
  }
};

#endif // NVM_OOPS_NVMOOPDESC_HPP

#endif // OUR_PERSIST
