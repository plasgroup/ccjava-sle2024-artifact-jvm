#ifdef OUR_PERSIST

#ifndef NVM_OURPERSIST_INLINE_HPP
#define NVM_OURPERSIST_INLINE_HPP

#include "nvm/ourPersist.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "oops/klass.hpp"
#include "oops/oop.inline.hpp"

inline bool OurPersist::enable() {
  // init
  if (OurPersist::_enable == our_persist_not_set) {
    bool enable_slow = Arguments::is_interpreter_only() && !UseCompressedOops;
    OurPersist::_enable = enable_slow ? our_persist_enable : our_persist_disable;
    if (OurPersist::_enable) {
      tty->print("OurPersist is enabled.\n");
    } else {
      tty->print("OurPersist is disabled.\n");
    }
  }

  bool res = OurPersist::_enable == our_persist_enable;
  assert(OurPersist::_enable != our_persist_not_set, "");
  return res;
}

inline bool OurPersist::is_target(Klass* klass) {
  assert(klass != NULL, "");
  assert(OurPersist::is_target_slow(klass) == OurPersist::is_target_fast(klass), "");

  return OurPersist::is_target_fast(klass);
}

#ifdef ASSERT
inline bool OurPersist::is_target_slow(Klass* klass) {
  while (klass != NULL) {
    if (!OurPersist::is_target_fast(klass)) {
      return false;
    }
    klass = klass->super();
  }

  return true;
}
#endif // ASSERT

inline bool OurPersist::is_target_fast(Klass* klass) {
  int klass_id = klass->id();

  // if (klass_id == InstanceMirrorKlassID) {
  //   return false;
  // }
  // if (klass_id == InstanceRefKlassID) {
  //   return false;
  // }
  // if (klass_id == InstanceClassLoaderKlassID) {
  //   return false;
  // }

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
