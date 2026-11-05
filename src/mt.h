#pragma once

#include <cstdint>
#include <vector>

// MT Framework's own type records (MtDTI): every class of the engine and the
// game that derives from MtObject has one, a static object in the data
// sections, linked into a tree as the program starts:
//
//   +0x00 vtable   (the record's own: makes an instance of the class)
//   +0x04 name     ("uPlayer", "sMain", ...)
//   +0x08 next     (the next sibling)
//   +0x0C child    (the first derived class)
//   +0x10 parent
//   +0x14, +0x18   (0 in every record seen)
//   +0x1C size     (bits 0..22: the instance's size in bytes; bit 24 a flag)
//   +0x20 id       (a hash of the name - or, for a class the game makes from a
//                   key, the key: (kind << 16 | number) << 8 | variant)
//
// The tree's root is a record with no name; MtObject is its child. Every class
// with a record answers it from vtable slot 4 (getDTI: `mov eax, record; ret`),
// so a class's vtable is found from its name: the record, its getDTI, the one
// vtable that holds that function in slot 4. The mod finds the classes it uses
// this way - by name, never by address.
namespace re5cc::mt {

struct Class {
  uintptr_t dti = 0;      // the record
  uintptr_t vtable = 0;   // the class's vtable (slot 0 its destructor), 0 when not found
  const char* name = "";
  uint32_t size = 0;      // an instance's size in bytes
  uintptr_t parent = 0;   // the parent's record
  uint32_t id = 0;        // the record's id: the key the game looks its classes up by
};

// From the first moment the game's code is unpacked: finds MtObject's record and
// the root above it, and indexes every record under it. False (and the log says
// why) when the records are not there or not as described above.
bool init();
int class_count();

// The class of that name (nullptr when the game has none). The vtables are
// indexed on the first call: one pass over the code for every getDTI, one over
// the read-only data for the vtables holding them.
const Class* find(const char* name);

// The class with this record (no vtable search), or nullptr.
const Class* by_dti(uintptr_t dti);

// The class whose record has this id, or nullptr. The game makes some classes from
// a key rather than a name - an item's model class is keyed (7 << 16 | item) << 8
// (uWp13 for the MP5, uIt0201 for handgun ammo) - and finds the record by it.
const Class* by_id(uint32_t id);

// Every class, in name order (their vtables are looked for on the first call).
const Class* classes(int* count);

// Every vtable that answers the class's record from slot 4: its own, and those of
// classes without a record of their own that derive from it (MT Framework gives
// a record only to classes that need one).
std::vector<uintptr_t> vtables_of(const Class* c);

// The record an object's class answers (its vtable's slot 4 run as getDTI - only
// when that function is one of the `mov eax, record; ret` kind), or 0.
uintptr_t dti_of_object(uintptr_t obj);
// The name of the object's class, or null.
const char* class_name_of(uintptr_t obj);
// True when the object's class is `cls` or derives from it.
bool is_a(uintptr_t obj, const Class* cls);
bool derives(uintptr_t dti, uintptr_t ancestor_dti);

}  // namespace re5cc::mt
