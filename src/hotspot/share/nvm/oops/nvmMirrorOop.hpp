#ifdef OUR_PERSIST

#ifndef NVM_OOPS_NVMMIRROROOPDESC_HPP
#define NVM_OOPS_NVMMIRROROOPDESC_HPP

#include "memory/allStatic.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/ostream.hpp"
#include "oops/instanceMirrorKlass.inline.hpp"
#include "nvm/oops/nvmOop.hpp"

typedef class nvmMirrorOopDesc* nvmMirrorOop;
class nvmMirrorOopDesc : public nvmOopDesc {
  friend class VM_OurPersistRecoveryDramCopy; // DEBUG:
 private:
  static nvmMirrorOopDesc* class_list_head;
  static nvmMirrorOopDesc* class_list_tail;
  static pthread_mutex_t class_list_mtx;

  static void* nvm_head;
  static void* nvm_tail;
  static pthread_mutex_t allocate_mtx;

  static int offset_of_free_space();
  static int length_of_free_space();

  static int offset_of_class_list_next();
  static nvmMirrorOopDesc** class_list_next_addr(nvmMirrorOopDesc* target);

  static int offset_of_klass_name();
  static int length_of_klass_name();

  static void add_class_list(nvmMirrorOopDesc* target);

  void set_klass_name(char* name);
  void set_class_list_next(nvmMirrorOopDesc* next);

 public:
  char* klass_name();
  nvmMirrorOopDesc* class_list_next();

  static nvmMirrorOopDesc* create_mirror(Klass* klass, oop mirror);
  static nvmMirrorOopDesc* create_and_set_mirror(Klass* klass, oop mirror);
};

#endif // NVM_OOPS_NVMMIRROROOPDESC_HPP

#endif // OUR_PERSIST
