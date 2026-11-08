#include "boot.h"

#include <windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstring>

#include "cheats.h"
#include "config.h"
#include "crash.h"
#include "dispatch.h"
#include "fpu.h"
#include "game.h"
#include "image.h"
#include "input.h"
#include "inventory.h"
#include "itembox.h"
#include "log.h"
#include "menus.h"
#include "mem.h"

extern "C" IDirect3D9* WINAPI re5cc_Direct3DCreate9(UINT sdk);  // overlay_dx9.cpp

namespace re5cc::boot {
namespace {

volatile LONG g_done = 0;
char g_status[200] = "the game has not started its renderer yet";

using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
GetProcAddressFn g_orig_gpa = nullptr;
uintptr_t g_gpa_slot = 0;
volatile LONG g_gpa_calls = 0;

// With Trace = 1 the distinct names resolved through the hook are logged once
// each: what Enigma's loader asks for, in the order it asks.
constexpr int kSeen = 2048;
char g_seen[kSeen][40];
volatile LONG g_nseen = 0;

void trace_name(HMODULE module, const char* name) {
  if (!config::get().trace) return;
  const LONG n = g_nseen;
  for (LONG i = 0; i < n && i < kSeen; ++i)
    if (!std::strncmp(g_seen[i], name, sizeof(g_seen[i]) - 1)) return;
  const LONG idx = InterlockedIncrement(&g_nseen) - 1;
  if (idx >= kSeen) return;
  std::snprintf(g_seen[idx], sizeof(g_seen[idx]), "%s", name);
  char path[MAX_PATH] = "?";
  GetModuleFileNameA(module, path, MAX_PATH);
  const char* leaf = std::strrchr(path, '\\');
  logf("trace: GetProcAddress(%s, %s) thread %lu", leaf ? leaf + 1 : path, name, GetCurrentThreadId());
}

// What the game gets for an import of `name` that resolved to `real`: the
// mod's own function for the few it hooks, else `real`. `via` says which way
// Enigma asked, for the log.
FARPROC substitute(HMODULE module, const char* name, FARPROC real, const char* via) {
  if (!std::strcmp(name, "Direct3DCreate9")) {
    static volatile LONG logged = 0;
    if (!InterlockedExchange(&logged, 1)) {
      char path[MAX_PATH] = "?";
      GetModuleFileNameA(module, path, MAX_PATH);
      logf("boot: the game's Direct3DCreate9 resolved through %s (from %s) - it gets the mod's", via, path);
    }
    return reinterpret_cast<FARPROC>(&re5cc_Direct3DCreate9);
  }
  if (!config::get().disable_input)
    if (FARPROC w = input::wrap(name, real)) return w;
  return real;
}

// ntdll's LdrGetProcedureAddress, as Enigma's loader asks for it through the
// exe's GetProcAddress: Enigma resolves the game's own imports with it (they
// never come through GetProcAddress), so the address it hands back for those
// is where the mod's functions go in.
struct AnsiString {
  USHORT length, maximum;
  const char* buffer;
};
using LdrGetProcFn = LONG(WINAPI*)(HMODULE, const AnsiString*, ULONG, void**);
LdrGetProcFn g_real_ldr_get_proc = nullptr;
volatile LONG g_ldr_calls = 0;

LONG WINAPI hk_ldr_get_proc(HMODULE module, const AnsiString* name, ULONG ordinal, void** address) {
  const LONG status = g_real_ldr_get_proc(module, name, ordinal, address);
  InterlockedIncrement(&g_ldr_calls);
  if (status < 0 || !address || !*address || !name || !name->buffer || name->length >= 64) return status;
  char n[64];
  std::memcpy(n, name->buffer, name->length);
  n[name->length] = '\0';
  trace_name(module, n);
  // The game's CRT sets its exception filter as it starts: chained under ours. Its
  // own ways of ending itself are logged.
  if (!std::strcmp(n, "SetUnhandledExceptionFilter")) *address = crash_filter_hook();
  else if (void* ends = crash_exit_hook(n, *address)) *address = ends;
  else *address = reinterpret_cast<void*>(substitute(module, n, reinterpret_cast<FARPROC>(*address), "LdrGetProcedureAddress"));
  return status;
}

FARPROC WINAPI hk_get_proc_address(HMODULE module, LPCSTR name) {
  FARPROC real = g_orig_gpa(module, name);
  InterlockedIncrement(&g_gpa_calls);
  if (!real || !name || !HIWORD(reinterpret_cast<uintptr_t>(name))) return real;
  trace_name(module, name);
  if (!std::strcmp(name, "LdrGetProcedureAddress")) {
    if (!g_real_ldr_get_proc) g_real_ldr_get_proc = reinterpret_cast<LdrGetProcFn>(real);
    logf("boot: Enigma's loader asked for LdrGetProcedureAddress - handed it the mod's, which sees the game's imports");
    return reinterpret_cast<FARPROC>(&hk_ldr_get_proc);
  }
  return substitute(module, name, real, "the exe's GetProcAddress");
}

}  // namespace

bool install_loader_hook() {
  HMODULE exe = GetModuleHandleA(nullptr);
  const uintptr_t slot = mem::iat_slot(exe, "kernel32.dll", "GetProcAddress");
  void* prev = slot ? mem::read<void*>(slot) : nullptr;
  if (!prev || !mem::write<uintptr_t>(slot, reinterpret_cast<uintptr_t>(&hk_get_proc_address))) {
    logf("ERROR: boot: the exe's GetProcAddress import (Enigma's loader's) not found - the game's imports cannot be "
         "reached as they are resolved");
    return false;
  }
  g_orig_gpa = reinterpret_cast<GetProcAddressFn>(prev);
  g_gpa_slot = slot;
  image::note_hooked_slot(slot, reinterpret_cast<uintptr_t>(prev));
  logf("boot: the exe's GetProcAddress import (Enigma's loader's) hooked at exe+0x%X%s",
       static_cast<unsigned>(slot - reinterpret_cast<uintptr_t>(exe)), config::get().trace ? " - tracing names" : "");
  return true;
}

void remove_loader_hook() {
  if (g_gpa_slot && g_orig_gpa && mem::read<uintptr_t>(g_gpa_slot) == reinterpret_cast<uintptr_t>(&hk_get_proc_address))
    mem::write<uintptr_t>(g_gpa_slot, reinterpret_cast<uintptr_t>(g_orig_gpa));
}

namespace {

void start() {
  image::init();
  if (const char* problem = image::layout_problem()) {
    std::snprintf(g_status, sizeof(g_status), "%s - not the Steam version the mod was made for (build 11465250)", problem);
    logf("ERROR: boot: %s", g_status);
    InterlockedExchange(&g_done, 1);
    return;
  }
  if (!image::unpacked()) {
    std::snprintf(g_status, sizeof(g_status), "the game's code was not in memory when its renderer started");
    logf("ERROR: boot: %s - nothing hooked", g_status);
    InterlockedExchange(&g_done, 1);
    return;
  }
  const mem::Range iat = image::iat();
  logf("boot: the game's code is unpacked (thread %lu); its import table %08X..%08X (%u slots); %ld names went "
       "through the GetProcAddress hook, %ld through LdrGetProcedureAddress",
       GetCurrentThreadId(), static_cast<unsigned>(iat.begin), static_cast<unsigned>(iat.end),
       static_cast<unsigned>(iat.size() / 4), g_gpa_calls, g_ldr_calls);

  // The crash logger's chain: the CRT set its filter long ago (ours went back on
  // top in the tick); the game's later calls go through the hook.
  if (HMODULE k32 = GetModuleHandleA("kernel32.dll")) {
    image::hook_import("SetUnhandledExceptionFilter",
                       reinterpret_cast<void*>(GetProcAddress(k32, "SetUnhandledExceptionFilter")), crash_filter_hook());
    for (const char* end : {"FatalAppExitA", "ExitProcess"}) {
      void* real = reinterpret_cast<void*>(GetProcAddress(k32, end));
      if (void* hook = crash_exit_hook(end, real)) image::hook_import(end, real, hook);
    }
  }
  input::install();
  if (config::get().disable_game) {
    logf("boot: game hooks disabled by config");
  } else if (!game::discover()) {
    std::snprintf(g_status, sizeof(g_status), "%s", game::status_text());
  }
  if (game::ready()) {
    cheats::install();
    inventory::discover();
    itembox::discover();
    menus::discover();
  }
  if (!config::get().disable_tick && game::ready()) dispatch::install();
  if (config::get().dump_image) image::dump(config::dir());
  if (game::ready()) std::snprintf(g_status, sizeof(g_status), "ready");
  InterlockedExchange(&g_done, 1);
}

}  // namespace

// The game's main thread, in its first Direct3DCreate9: the boot runs on it, with the
// x87 and SSE units as the game's code and whatever ran before left them. Its own floats
// (and the C runtime's formatting of them) want every exception masked, none pending and
// an empty register stack: the guard sets that, and puts the game's own back after the
// boot (the game creates its device with D3DCREATE_FPU_PRESERVE, so nothing else does).
// A user's first run on Windows (2026-11-05) ended without a word at the boot's first float.
void unpacked() {
  fpu::Guard guard;
  char state[256];
  fpu::describe(guard.found(), guard.found_mxcsr(), state, sizeof(state));
  const bool usual = fpu::usual(guard.found(), guard.found_mxcsr());
  logf("boot: the game's thread brings %s%s", state,
       usual ? " - nothing in it that would fault a float"
             : " (the mod's own code runs with every exception masked and the stack empty, and the game's state is "
               "put back after)");
  reassert_crash_filter();
  start();
  const fpu::Env e = fpu::env();
  const uint32_t m = fpu::mxcsr();
  if (!fpu::as_set(e, m)) {
    fpu::describe(e, m, state, sizeof(state));
    logf("boot: the thread's x87/SSE unit changed during the boot (to %s): a system call the boot made (a hook in "
         "it) - the game's own state is put back all the same",
         state);
  }
}

bool done() { return g_done != 0; }
const char* status() { return g_status; }

}  // namespace re5cc::boot
