// The mod ships as d3d9.dll. Every function d3d9.dll exports but one is a
// one-instruction thunk that jumps through a table of the real DLL's addresses -
// the stack and every register untouched, so the callee sees exactly the call
// the game made. Direct3DCreate9 is the mod's own (overlay_dx9.cpp): it calls
// the real one and hooks what it hands back.
//
// Why d3d9: re5dx9.exe is wrapped by Enigma Protector, and the wrapper carries
// the game's steam_api.dll inside the exe and loads it from there - there is no
// such file to stand in for. Of the DLLs the exe imports, d3d9.dll is the one
// that loads from the game's folder with no launch option everywhere: Windows
// looks in the program's folder first (d3d9 is not a "known DLL"), and Proton
// runs DXVK's d3d9.dll as a native DLL (its own WINEDLLOVERRIDES say d3d9=n), so
// a d3d9.dll beside the exe comes first there too. dinput8, xinput1_3, winmm,
// dsound and the rest are Wine builtins, which Proton would load in place of
// ours. The exe imports d3d9.dll statically, so this DLL is loaded - and DllMain
// runs - before the wrapper has unpacked any of the game's code.
//
// The real d3d9.dll is the system's (Windows' own, or DXVK's under Proton),
// loaded by its full path - or, when the player already had another d3d9.dll in
// the folder (a graphics mod such as ReShade), that one renamed d3d9_orig.dll,
// which is then in front of the system's as before.

#include "proxy.h"

#include <cstdio>
#include <cstring>

#include "log.h"
#include "mem.h"
#include "proxy_exports.inc"

// Plain C symbols so the assembly below can name them (with the 32-bit underscore).
extern "C" {
void* g_proxy_orig[PROXY_EXPORT_COUNT] = {};
}

// One thunk per export: `jmp [g_proxy_orig + i*4]`, defined at file scope in
// assembly so no prologue or epilogue is ever emitted around the jump.
#define PROXY_THUNK(i, name, args)                             \
  asm(".text\n"                                                \
      ".globl _d3d9_" #i "\n"                                  \
      "_d3d9_" #i ":\n"                                        \
      "\tjmp *_g_proxy_orig+" #i "*4\n");
PROXY_EXPORTS(PROXY_THUNK)
#undef PROXY_THUNK

// What a function the real DLL lacks answers: 0 in eax, its arguments taken off
// the stack (d3d9's exports are __stdcall). The game imports none of them - what
// it imports the real DLL must have - so only something resolving a name at run
// time can get here.
#define PROXY_STUB(i, name, args)                              \
  asm(".text\n"                                                \
      ".globl _d3d9_missing_" #i "\n"                          \
      "_d3d9_missing_" #i ":\n"                                \
      "\txorl %eax, %eax\n"                                    \
      "\tret $" #args "\n");
PROXY_EXPORTS(PROXY_STUB)
#undef PROXY_STUB

extern "C" {
#define PROXY_DECL(i, name, args) void d3d9_missing_##i();
PROXY_EXPORTS(PROXY_DECL)
#undef PROXY_DECL
}

namespace re5cc::proxy {
namespace {

constexpr const char* const kNames[] = {
#define PROXY_NAME(i, name, args) name,
    PROXY_EXPORTS(PROXY_NAME)
#undef PROXY_NAME
};
void* const kMissing[] = {
#define PROXY_MISSING(i, name, args) reinterpret_cast<void*>(&d3d9_missing_##i),
    PROXY_EXPORTS(PROXY_MISSING)
#undef PROXY_MISSING
};
// What re5dx9.exe imports from d3d9.dll (build 11465250): without these the game
// cannot run, with the mod or without it.
constexpr const char* const kGameImports[] = {"Direct3DCreate9", "D3DPERF_GetStatus"};

HMODULE g_orig = nullptr;
char g_failure[1024] = "";

// The module this code is in: the mod's DLL.
HMODULE self() {
  HMODULE m = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     reinterpret_cast<LPCSTR>(&self), &m);
  return m;
}

}  // namespace

HMODULE original() { return g_orig; }
const char* failure() { return g_failure; }
void* real(const char* name) { return g_orig ? reinterpret_cast<void*>(GetProcAddress(g_orig, name)) : nullptr; }

bool load_original(const char* dir) {
  g_orig = nullptr;
  g_failure[0] = '\0';

  // Another d3d9.dll the player had in the folder, renamed: it stays in front of
  // the system's, as it was before the mod came.
  char path[MAX_PATH] = {};
  std::snprintf(path, sizeof(path), "%sd3d9_orig.dll", dir);
  bool chained = GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
  if (!chained) {
    char sys[MAX_PATH] = {};
    const UINT n = GetSystemDirectoryA(sys, MAX_PATH);
    if (!n || n >= MAX_PATH) {
      logf("FATAL: GetSystemDirectory failed (GetLastError=%lu)", GetLastError());
      std::snprintf(g_failure, sizeof(g_failure),
                    "RE5CabbyCodes: Windows did not say where its system folder is, so the mod cannot find the "
                    "real d3d9.dll to pass the game's calls on to.\n\nTo play without the mod, delete the mod's "
                    "d3d9.dll from the game's folder.");
      return false;
    }
    std::snprintf(path, sizeof(path), "%s\\d3d9.dll", sys);
  }
  HMODULE orig = LoadLibraryA(path);
  if (!orig || orig == self()) {
    const DWORD error = orig ? 0 : GetLastError();
    logf("FATAL: could not load the real d3d9.dll from %s (%s)", path,
         orig ? "it is the mod itself" : "LoadLibrary failed");
    if (chained)
      std::snprintf(g_failure, sizeof(g_failure),
                    "RE5CabbyCodes: d3d9_orig.dll could not be loaded%s.\n\n"
                    "The mod passes the game's graphics calls on to d3d9_orig.dll when there is one beside it - "
                    "another d3d9.dll you had in the game's folder (a graphics mod), renamed. %s\n\n"
                    "To fix it: delete d3d9_orig.dll (or put a working copy of your graphics mod's d3d9.dll back "
                    "under that name).",
                    orig ? " - it is a copy of the mod itself" : "", orig ? "" : "Windows could not load it.");
    else
      std::snprintf(g_failure, sizeof(g_failure),
                    "RE5CabbyCodes: Windows' own d3d9.dll could not be loaded (%s, error %lu).\n\n"
                    "The mod passes the game's graphics calls on to it. Without it the game cannot run - with the "
                    "mod or without it.",
                    path, error);
    return false;
  }
  // Pin it: nothing may drop the last reference while the game still uses it.
  HMODULE pinned = nullptr;
  GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                     reinterpret_cast<LPCSTR>(orig), &pinned);

  for (const char* name : kGameImports) {
    if (GetProcAddress(orig, name)) continue;
    logf("FATAL: %s has no %s, which the game imports", path, name);
    std::snprintf(g_failure, sizeof(g_failure),
                  "RE5CabbyCodes: %s has no %s, which the game needs.\n\n%s", path, name,
                  chained ? "It is not a working d3d9.dll. To fix it: delete d3d9_orig.dll."
                          : "This d3d9.dll cannot run the game, with the mod or without it.");
    return false;
  }

  int n_missing = 0;
  char listed[200] = "";  // the first few missing, for the log
  for (int i = 0; i < PROXY_EXPORT_COUNT; ++i) {
    void* fn = reinterpret_cast<void*>(GetProcAddress(orig, kNames[i]));
    if (!fn) {
      fn = kMissing[i];
      if (++n_missing <= 4) {
        const size_t at = std::strlen(listed);
        std::snprintf(listed + at, sizeof(listed) - at, "%s%s", at ? ", " : "", kNames[i]);
      }
    }
    g_proxy_orig[i] = fn;
  }
  g_orig = orig;
  char real_path[MAX_PATH] = "?";
  GetModuleFileNameA(orig, real_path, MAX_PATH);
  logf("proxy: forwarding d3d9.dll to %s%s (%d of %d functions there%s%s%s)", real_path,
       chained ? " - another d3d9.dll the player had, renamed d3d9_orig.dll" : "", PROXY_EXPORT_COUNT + 1 - n_missing,
       PROXY_EXPORT_COUNT + 1, n_missing ? "; missing, answering 0: " : "", listed, n_missing > 4 ? ", ..." : "");
  return true;
}

}  // namespace re5cc::proxy
