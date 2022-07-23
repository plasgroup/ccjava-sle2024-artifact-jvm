#include "nvm/oops/nvmMirrorOop.hpp"

#include "nvm/nvmDebug.hpp"
#include "oops/fieldStreams.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.hpp"
#include "oops/nvmHeader.inline.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "oops/typeArrayKlass.hpp"

nvmMirrorOopDesc* nvmMirrorOopDesc::class_list_head = NULL;
nvmMirrorOopDesc* nvmMirrorOopDesc::class_list_tail = NULL;
pthread_mutex_t nvmMirrorOopDesc::class_list_mtx  = PTHREAD_MUTEX_INITIALIZER;

void* nvmMirrorOopDesc::nvm_head = NULL;
void* nvmMirrorOopDesc::nvm_tail = NULL;
pthread_mutex_t nvmMirrorOopDesc::allocate_mtx = PTHREAD_MUTEX_INITIALIZER;

int nvmMirrorOopDesc::offset_of_free_space() {
  int start = oopDesc::header_size() * HeapWordSize;
  return start + 2 * HeapWordSize;
}

int nvmMirrorOopDesc::length_of_free_space() {
  int end = InstanceMirrorKlass::offset_of_static_fields() - HeapWordSize;
  int size = end - nvmMirrorOopDesc::offset_of_free_space();
  return size;
}

int nvmMirrorOopDesc::offset_of_class_list_next() {
  int start = nvmMirrorOopDesc::offset_of_free_space();
  return start;
}

nvmMirrorOopDesc** nvmMirrorOopDesc::class_list_next_addr(nvmMirrorOopDesc* target) {
  void* addr = ((char*)target) + nvmMirrorOopDesc::offset_of_class_list_next();
  return (nvmMirrorOopDesc**)addr;
  //return &target->_class_list_next;
}

int nvmMirrorOopDesc::offset_of_klass_name() {
  int start = nvmMirrorOopDesc::offset_of_class_list_next() + HeapWordSize;
  return start;
}

int nvmMirrorOopDesc::length_of_klass_name() {
  int size = nvmMirrorOopDesc::length_of_free_space() - HeapWordSize;
  return size;
}

char* nvmMirrorOopDesc::klass_name() {
  char* name = ((char*)this) + nvmMirrorOopDesc::offset_of_klass_name();
  return name;
  //return this->debug;
}

void nvmMirrorOopDesc::set_klass_name(char* name) {
  int max_len = nvmMirrorOopDesc::length_of_klass_name();
  if (strlen(name) + 1 >= (size_t)max_len) {
    assert(false, "");
    return; // TODO:
  }

  //tty->print_cr("set_klass_name: %s", name);

  char* addr = ((char*)this) + nvmMirrorOopDesc::offset_of_klass_name();
  //char* addr = AllocateHeap(strlen(name) + 1, mtNone);
  //this->debug = addr;
  strncpy(addr, name, max_len);
  //strncpy(addr, name, strlen(name));
  addr[strlen(name)] = '\0';
}

nvmMirrorOopDesc* nvmMirrorOopDesc::class_list_next() {
  nvmMirrorOopDesc** ptr = nvmMirrorOopDesc::class_list_next_addr(this);
  return *ptr;
}

void nvmMirrorOopDesc::set_class_list_next(nvmMirrorOopDesc* next) {
  nvmMirrorOopDesc** ptr = nvmMirrorOopDesc::class_list_next_addr(this);
  *ptr = next;
}

void nvmMirrorOopDesc::add_class_list(nvmMirrorOopDesc* target) {
  assert(target != NULL, "");
  pthread_mutex_lock(&class_list_mtx);

  if (class_list_head == NULL) {
    class_list_head = target;
    // DEBUG:
    tty->print_cr("class_list_head: %p", class_list_head);
  } else {
    assert(class_list_tail != NULL, "");
    class_list_tail->set_class_list_next(target);
  }

  target->set_class_list_next(NULL);
  class_list_tail = target;
  NVM_WRITEBACK(nvmMirrorOopDesc::class_list_next_addr(target));

  if (false) {
    nvmMirrorOopDesc* cur = (nvmMirrorOopDesc*)0x700000000000;
    tty->print_cr("");
    tty->print_cr("class_list_tail: %p", class_list_tail);
    //nvmMirrorOopDesc* cur = class_list_head;
    while (cur != NULL) {
      tty->print_cr("%s", cur->klass_name());
      cur = cur->class_list_next();
    }
  }

  pthread_mutex_unlock(&class_list_mtx);
}

void nvmMirrorOopDesc::mirror_create(Klass* klass, oop mirror) {
  const DecoratorSet ds = MO_UNORDERED | AS_NORMAL | IN_HEAP;
  typedef BarrierSet::AccessBarrier<ds, NVMCardTableBarrierSet> Raw;

  assert(OurPersist::enable(), "should be enabled");
  assert(klass != NULL && mirror != NULL, "");
  assert(klass->java_mirror() != NULL, "");
  assert(klass->java_mirror()->nvm_header().to_pointer() == NULL, "");

  //tty->print_cr("offset_of_free_space: %d", nvmMirrorOopDesc::offset_of_free_space());
  //tty->print_cr("length_of_free_space: %d", nvmMirrorOopDesc::length_of_free_space());
  //tty->print_cr("offset_of_klass_name: %d", nvmMirrorOopDesc::offset_of_klass_name());
  //tty->print_cr("length_of_klass_name: %d", nvmMirrorOopDesc::length_of_klass_name());

  // DEBUG:
  if (class_list_head == NULL && true) {
    NVMAllocator::init();
    nvmMirrorOopDesc* cur = (nvmMirrorOopDesc*)0x700000000000;
    int* ptr = (int*)cur;
    tty->print_cr("test: %d", *ptr);
    while (cur != NULL) {
      tty->print_cr("%s", cur->klass_name());
      cur = cur->class_list_next();
    }
    //assert(false, "end");
  }

  // Persistence of the mirror object
  void* nvm_mirror = NVMAllocator::allocate(mirror->size());
  nvmHeader::set_header(oop(nvm_mirror), nvmHeader::zero());
  oop(nvm_mirror)->set_mark(markWord::zero());
  memset(nvm_mirror, 0, mirror->size() * HeapWordSize); // TODO: when debug mode only?

  if (klass->is_instance_klass()) {
    InstanceKlass* ik = InstanceKlass::cast(klass);
    for (JavaFieldStream fs(ik); !fs.done(); fs.next()) {
      fieldDescriptor& fd = fs.field_descriptor();
      if (!fd.is_durableroot()) continue;
      assert(fd.is_static() && is_reference_type(fd.field_type()), "");
      assert(!fd.is_static() || (int)fd.offset() >= InstanceMirrorKlass::offset_of_static_fields(), "");

      oop v = Raw::oop_load_in_heap_at(mirror, fd.offset());
      //if (fd.has_initial_value()) continue;
      if (v != NULL) {
        OurPersist::ensure_recoverable(v);
        Raw::oop_store_in_heap_at(oop(nvm_mirror), fd.offset(), oop(v->nvm_header().fwd()));
        if (v->klass()->name()->equals("java/lang/String")) continue;
        klass->print();
        mirror->print();
        v->print();
      }
      assert(v == NULL, "v: %p, off: %d", OOP_TO_VOID(v), fd.offset());

      assert(NVMDebug::cmp_dram_and_nvm_val(mirror, oop(nvm_mirror), fd.offset(),
                                            fd.field_type(), fd.access_flags()), "check nvm heap");
    }
  }

  for (int i = oopDesc::header_size() * HeapWordSize; i < InstanceMirrorKlass::offset_of_static_fields(); i++) {
    char v = oop(nvm_mirror)->char_field(i);
    //assert(v == 0, "nvm mirror klass has non-zero field: %d\n", i);
  }

  nvmMirrorOopDesc* nvm_java_class = ((nvmMirrorOopDesc*)((uintptr_t)nvm_mirror));
  nvm_java_class->set_klass_name(klass->name()->as_C_string());
  NVM_WRITEBACK_LOOP(nvm_java_class, mirror->size());
  nvmMirrorOopDesc::add_class_list(nvm_java_class);
  {
    char* name1 = klass->name()->as_C_string();
    char* name2 = nvm_java_class->klass_name();
    assert(strcmp(name1, name2) == 0, "should be same: %s != %s", name1, name2);
  }

  nvmHeader::set_fwd(mirror, nvm_mirror);

  // Update the durableroot list
/*
  void* head = OurPersist::durableroot_list_head();
  void* tail = OurPersist::durableroot_list_tail();
  if (head == NULL) {
    assert(tail == NULL, "");

    OurPersist::set_durableroot_list_head(nvm_mirror);
  } else {
    assert(nvmHeader::is_fwd(nvmHeader::from_pointer(head).fwd()), "");
    assert(nvmHeader::is_fwd(nvmHeader::from_pointer(tail).fwd()), "");

    nvmHeader::set_fwd(oop(tail), nvm_mirror);
  }
  OurPersist::set_durableroot_list_tail(nvm_mirror);
*/

/*
  // Initialize durableroot fields to NULL and set the non-durableroot flag
  // (all bits to 1) for reference types only
  if (klass->is_instance_klass()) {
    InstanceKlass* ik = InstanceKlass::cast(klass);
    for (JavaFieldStream fs(ik); !fs.done(); fs.next()) {
      fieldDescriptor& fd = fs.field_descriptor();
      if (!fd.is_static()) continue;
      if (!is_reference_type(fd.field_type())) continue;

      void* init_value = fd.is_durableroot() ? NULL : (void*)~uintptr_t(0);
      Raw::oop_store_in_heap_at(oop(nvm_mirror), fd.offset(), oop(init_value));
    }
  } else {
#ifdef ASSERT
    int word_size = klass->java_mirror()->size();
    int byte_size = word_size * HeapWordSize;
    int off = InstanceMirrorKlass::offset_of_static_fields();
    //tty->print_cr(" byte_size: %d, off: %d", byte_size, off);
    assert(byte_size == off, "");
#endif // ASSERT
  }
*/
}
