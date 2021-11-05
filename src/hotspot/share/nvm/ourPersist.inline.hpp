#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_INLINE_HPP
#define NVM_OURPERSIST_INLINE_HPP

#include "nvm/ourPersist.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"

inline bool OurPersist::is_target(Klass* klass) {
  assert(klass != NULL, "");
  int klass_id = klass->id();

  // DEBUG:
  //return true;

  if (klass_id == InstanceMirrorKlassID) {
    return false;
  }
  if (klass_id == InstanceRefKlassID) {
    return false;
  }
  if (klass_id == InstanceClassLoaderKlassID) {
    return false;
  }

  Klass* super = klass->super();
  if (super != NULL) {
    return OurPersist::is_target(super);
  }

  return true;
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
