#pragma once

#include <windows.h>

#include <cstdint>

#include "mem.h"

// re5dx9.exe as it is in memory. The exe is wrapped by Enigma Protector (and
// SteamStub over that): on disk its code is compressed, and the wrapper unpacks
// it in memory - after the mod's DllMain has run - and fills the game's import
// table with the real addresses (no names: the table it rebuilds has none). The
// image keeps the game's own sections at their places, with blank names: the
// code, the read-only data (the import table at its start), the data and a
// fourth, then the resources; Enigma's and SteamStub's own follow.
//
// Nothing here reads the game's code before unpacked() says it is there: the
// game's first call of Direct3DCreate9 is the mod's first moment (the game's own
// code runs then).
namespace re5cc::image {

void init();                // the exe's sections (cheap; from DllMain)
uintptr_t base();
HMODULE module();
mem::Range text();          // the code
mem::Range rdata();         // read-only data: the import table, vtables, strings
mem::Range data();          // the data and the fourth section (the class records live there)
mem::Range whole();         // the game's sections, headers to .rsrc
const char* layout_problem();  // null when the sections are as build 11465250 has them

// The code is in place: its padding and prologues read as code, and the import
// table holds addresses of loaded modules.
bool unpacked();

// The import table Enigma rebuilt: the slots from .rdata's start to the first
// value that is neither 0 nor an address in a loaded module.
mem::Range iat();

// The slot of the game's import table that holds `fn` (0 when none does), and the
// hook of one: the slot re-pointed at `hook`, its value before handed back.
uintptr_t slot_of(void* fn);
void* hook_import(const char* what, void* fn, void* hook, uintptr_t* slot_out = nullptr);

// The DumpImage diagnostic: the unpacked image beside the DLL
// (re5dx9.dumped.exe, raw offsets == RVAs) and its import map
// (re5dx9.imports.txt: each slot's module!export), for tools/fix_dump.py.
bool dump(const char* dir);

// A slot's value before one of the mod's import hooks, so a dump shows the game's.
void note_hooked_slot(uintptr_t slot, uintptr_t original);

}  // namespace re5cc::image
