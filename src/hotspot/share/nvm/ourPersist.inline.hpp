#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_INLINE_HPP
#define NVM_OURPERSIST_INLINE_HPP

#include "nvm/ourPersist.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"

inline bool OurPersist::enable() {
  if (!OurPersist::_enable_is_set) {
    OurPersist::_enable = Arguments::is_interpreter_only() && (!UseCompressedOops);
    OurPersist::_enable_is_set = true;
  }
  return OurPersist::_enable;
}

inline bool OurPersist::is_static_field(oop obj, ptrdiff_t offset) {
  Klass* k = obj->klass();
  if (k->id() != InstanceMirrorKlassID) {
    return false;
  }
  if (offset < InstanceMirrorKlass::offset_of_static_fields()) {
    return false;
  }
  return true;
}

#endif // NVM_OURPERSIST_INLINE_HPP

#endif // OUR_PERSIST
