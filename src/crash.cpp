#include "crash.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "fpu.h"
#include "image.h"
#include "log.h"
#include "mem.h"

namespace re5cc {
namespace {

LPTOP_LEVEL_EXCEPTION_FILTER g_previous = nullptr;
using SetFilterFn = LPTOP_LEVEL_EXCEPTION_FILTER(WINAPI*)(LPTOP_LEVEL_EXCEPTION_FILTER);
SetFilterFn g_orig_set_filter = nullptr;

const char* code_name(DWORD code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
    case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INT_DIVIDE_BY_ZERO";
    case EXCEPTION_PRIV_INSTRUCTION: return "PRIV_INSTRUCTION";
    case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "FLT_DENORMAL_OPERAND";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "FLT_DIVIDE_BY_ZERO";
    case EXCEPTION_FLT_INEXACT_RESULT: return "FLT_INEXACT_RESULT";
    case EXCEPTION_FLT_INVALID_OPERATION: return "FLT_INVALID_OPERATION";
    case EXCEPTION_FLT_OVERFLOW: return "FLT_OVERFLOW";
    case EXCEPTION_FLT_STACK_CHECK: return "FLT_STACK_CHECK";
    case EXCEPTION_FLT_UNDERFLOW: return "FLT_UNDERFLOW";
    case 0xC0000409: return "STACK_BUFFER_OVERRUN";
    case 0xC0000374: return "HEAP_CORRUPTION";
    default: return "exception";
  }
}

bool is_float(DWORD code) { return code >= EXCEPTION_FLT_DENORMAL_OPERAND && code <= EXCEPTION_FLT_UNDERFLOW; }

// A floating-point exception's x87 state, as the context has it.
void log_float_state(const EXCEPTION_POINTERS* info) {
  const CONTEXT* c = info->ContextRecord;
  if (!c || !is_float(info->ExceptionRecord->ExceptionCode) ||
      (c->ContextFlags & CONTEXT_FLOATING_POINT) != CONTEXT_FLOATING_POINT)
    return;
  fpu::Env e{};
  e.cw = c->FloatSave.ControlWord;
  e.sw = c->FloatSave.StatusWord;
  e.tw = c->FloatSave.TagWord;
  char what[256];
  fpu::describe(e, 0, what, sizeof(what));
  logf("       %s", what);
}

void describe(void* addr, char* out, size_t n) {
  HMODULE owner = nullptr;
  char path[MAX_PATH] = "?";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                             GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                         static_cast<LPCSTR>(addr), &owner) &&
      owner) {
    GetModuleFileNameA(owner, path, MAX_PATH);
    const char* leaf = std::strrchr(path, '\\');
    std::snprintf(out, n, "%s+0x%X", leaf ? leaf + 1 : path,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(addr) -
                                        reinterpret_cast<uintptr_t>(owner)));
  } else {
    std::snprintf(out, n, "%p (no module)", addr);
  }
}

// Return addresses off the stack from `sp` up, `depth` slots at most: up to `most`
// of the values that land in `code` - in any module when it is empty.
void log_stack(void** sp, int depth, int most, mem::Range code) {
  char where[MAX_PATH + 32];
  for (int i = 0, shown = 0; i < depth && shown < most; ++i) {
    if (!mem::readable(sp + i, sizeof(void*))) break;
    void* v = sp[i];
    HMODULE owner = nullptr;
    const bool in = code.empty() ? v && GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                                            static_cast<LPCSTR>(v), &owner) &&
                                         owner
                                  : code.contains(reinterpret_cast<uintptr_t>(v));
    if (!in) continue;
    describe(v, where, sizeof(where));
    logf("       stack[%03d] %s", i, where);
    ++shown;
  }
}

// The exception's details under its first line: what an access violation touched, the
// registers, a floating-point exception's x87 state, the stack's code addresses.
void log_details(const EXCEPTION_POINTERS* info) {
  // For an access violation the record carries what was touched and how; a
  // null or freed address there is the difference between a bad pointer and
  // a bad instruction, and it is not in the register dump below.
  if (info->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
      info->ExceptionRecord->NumberParameters >= 2) {
    const ULONG_PTR kind = info->ExceptionRecord->ExceptionInformation[0];
    logf("       %s address %08lX", kind == 1 ? "writing" : kind == 8 ? "executing" : "reading",
         static_cast<unsigned long>(info->ExceptionRecord->ExceptionInformation[1]));
  }
  if (info->ContextRecord) {
    logf("       eip=%08lX esp=%08lX ebp=%08lX eax=%08lX ecx=%08lX edx=%08lX",
         info->ContextRecord->Eip, info->ContextRecord->Esp, info->ContextRecord->Ebp,
         info->ContextRecord->Eax, info->ContextRecord->Ecx, info->ContextRecord->Edx);
    log_float_state(info);
    // A few return addresses off the stack, for the ones that land in a module.
    log_stack(reinterpret_cast<void**>(info->ContextRecord->Esp), 128, 8, {});
  }
}

LONG WINAPI on_exception(EXCEPTION_POINTERS* info) {
  if (info && info->ExceptionRecord) {
    void* at = info->ExceptionRecord->ExceptionAddress;
    char where[MAX_PATH + 32];
    describe(at, where, sizeof(where));
    logf("CRASH: %s (0x%08lX) at %s (thread %lu)", code_name(info->ExceptionRecord->ExceptionCode),
         info->ExceptionRecord->ExceptionCode, where, GetCurrentThreadId());
    log_details(info);
  }
  if (!g_previous) return EXCEPTION_CONTINUE_SEARCH;
  // A filter we chain to can handle an exception raised on purpose; that is no
  // crash, and the log should say so.
  const LONG r = g_previous(info);
  if (r == EXCEPTION_CONTINUE_EXECUTION) logf("       (the filter we chain to handled it and the program went on - not a crash)");
  return r;
}

// The top-level filter sees an exception only once nothing up the stack took it, and
// only while it is the filter: another DLL may have set its own since, or an exception
// handler of the game's or the protector's may end the program first. Windows' first
// report of an exception (a vectored handler, before any of those) is logged here for
// the mod's own: an error-class exception in its code, or in a function it called (its
// return address among the first slots of the stack). Nothing is handled here.
uintptr_t g_self_begin = 0, g_self_end = 0;
volatile LONG g_seen = 0;
volatile LONG g_in_first_chance = 0;
constexpr LONG kSeenMost = 4;
constexpr int kCallerSlots = 16;

void find_self() {
  HMODULE self = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&find_self), &self) ||
      !self)
    return;
  const auto base = reinterpret_cast<uintptr_t>(self);
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  g_self_begin = base;
  g_self_end = base + nt->OptionalHeader.SizeOfImage;
}

bool in_self(uintptr_t a) { return a >= g_self_begin && a < g_self_end; }

// 0: not the mod's; 1: in its code; 2: in a function it called.
int mods_own(const EXCEPTION_POINTERS* info) {
  if (in_self(reinterpret_cast<uintptr_t>(info->ExceptionRecord->ExceptionAddress))) return 1;
  if (!info->ContextRecord) return 0;
  auto** sp = reinterpret_cast<void**>(info->ContextRecord->Esp);
  if (!mem::readable(sp, kCallerSlots * sizeof(void*))) return 0;
  for (int i = 0; i < kCallerSlots; ++i)
    if (in_self(reinterpret_cast<uintptr_t>(sp[i]))) return 2;
  return 0;
}

LONG CALLBACK on_first_chance(EXCEPTION_POINTERS* info) {
  if (!info || !info->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
  const DWORD code = info->ExceptionRecord->ExceptionCode;
  // Errors only: not a breakpoint, a debugger's message, a thread's name or a C++ throw.
  if ((code & 0xF0000000) != 0xC0000000 || g_seen >= kSeenMost) return EXCEPTION_CONTINUE_SEARCH;
  if (!g_self_end) find_self();
  const int own = mods_own(info);
  if (!own || InterlockedExchange(&g_in_first_chance, 1)) return EXCEPTION_CONTINUE_SEARCH;
  if (InterlockedIncrement(&g_seen) <= kSeenMost) {
    char where[MAX_PATH + 32];
    describe(info->ExceptionRecord->ExceptionAddress, where, sizeof(where));
    logf("exception %s, first reported (before any handler, which may yet take it): %s (0x%08lX) at %s (thread %lu)",
         own == 2 ? "in a call the mod made" : "in the mod's own code", code_name(code), code, where,
         GetCurrentThreadId());
    log_details(info);
  }
  InterlockedExchange(&g_in_first_chance, 0);
  return EXCEPTION_CONTINUE_SEARCH;
}

// SetUnhandledExceptionFilter has one global slot, and the game's CRT takes it
// during start-up, after our DllMain has run - and a CRT's fatal-error path
// sets it to null on purpose before calling UnhandledExceptionFilter, so that
// nothing intercepts the report. Either way ours would never run and a crash
// would leave no line in the log. re5dx9.exe's import of it only exists once
// Enigma has unpacked the image and filled the import table (after DllMain):
// it is re-pointed at this hook then, and reassert_crash_filter() covers every
// path we do not see (the CRT's own call came before it).
LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hk_set_filter(LPTOP_LEVEL_EXCEPTION_FILTER next) {
  LPTOP_LEVEL_EXCEPTION_FILTER previous = g_previous;
  g_previous = next;
  if (g_orig_set_filter) g_orig_set_filter(&on_exception);
  logf("crash: the game set its top-level exception filter to %p; ours stays on top (chaining to it)", next);
  return previous;
}

// The game's own ways of ending leave the filter above nothing to see. Its fatal
// error (FUN_006CFEF0) hands its text to FatalAppExitA - a message box, then the
// end: a file it could not open, a Direct3D call that failed, an archive that would
// not unpack. The C runtime's own errors (a pure virtual call, abort) and a quit end
// in ExitProcess. Each is logged, with the game's code it came through, then goes on.
using FatalAppExitFn = void(WINAPI*)(UINT, LPCSTR);
using ExitProcessFn = void(WINAPI*)(UINT);
FatalAppExitFn g_real_fatal = nullptr;
ExitProcessFn g_real_exit = nullptr;

// The game's code up the stack from a hook's return address. The fatal error's text
// (its 1 KB buffer) lies between its handler and its caller: the walk goes past it.
void log_callers(void** sp) { log_stack(sp, 2048, 10, image::text()); }

void WINAPI hk_fatal_app_exit(UINT action, LPCSTR text) {
  char msg[400] = "(none)";
  if (text && mem::readable(text, 1)) {
    std::snprintf(msg, sizeof(msg), "%.*s", static_cast<int>(sizeof(msg) - 1), text);
    for (size_t n = std::strlen(msg); n && (msg[n - 1] == '\n' || msg[n - 1] == '\r'); --n) msg[n - 1] = '\0';
  }
  logf("FATAL: the game ends itself with its own error: \"%s\"", msg);
  log_callers(static_cast<void**>(__builtin_frame_address(0)) + 1);
  g_real_fatal(action, text);
}

void WINAPI hk_exit_process(UINT code) {
  logf("the game ends: ExitProcess(%u)%s", code, code ? " - not a quit's 0; where it came from:" : "");
  if (code) log_callers(static_cast<void**>(__builtin_frame_address(0)) + 1);
  g_real_exit(code);
}

}  // namespace

void describe_address(void* addr, char* out, unsigned n) { describe(addr, out, n); }

void install_crash_logger() {
  g_orig_set_filter = &SetUnhandledExceptionFilter;
  g_previous = SetUnhandledExceptionFilter(&on_exception);
  find_self();
  const bool first = AddVectoredExceptionHandler(1, &on_first_chance) != nullptr;
  logf("crash logger installed (the game's own filter is chained once it sets one%s)",
       first ? "; a fault in the mod's code is logged as it happens, before any handler" : "");
}

void* crash_filter_hook() { return reinterpret_cast<void*>(&hk_set_filter); }

void* crash_exit_hook(const char* name, void* real) {
  if (!name || !real) return nullptr;
  if (!std::strcmp(name, "FatalAppExitA")) {
    if (!g_real_fatal) g_real_fatal = reinterpret_cast<FatalAppExitFn>(real);
    return reinterpret_cast<void*>(&hk_fatal_app_exit);
  }
  if (!std::strcmp(name, "ExitProcess")) {
    if (!g_real_exit) g_real_exit = reinterpret_cast<ExitProcessFn>(real);
    return reinterpret_cast<void*>(&hk_exit_process);
  }
  return nullptr;
}

void reassert_crash_filter() {
  LPTOP_LEVEL_EXCEPTION_FILTER cur = SetUnhandledExceptionFilter(&on_exception);
  if (cur == &on_exception) return;
  if (cur) g_previous = cur;
  char whose[MAX_PATH + 32] = "none";
  if (cur) describe(reinterpret_cast<void*>(cur), whose, sizeof(whose));
  logf("crash: the top-level filter had been replaced (%p, %s) - ours is back on top%s", reinterpret_cast<void*>(cur),
       whose, cur ? ", chaining to it" : "");
}

bool where_is(unsigned long tid, char* out, unsigned n) {
  constexpr int kSlots = 512, kShown = 8;
  if (!n) return false;
  out[0] = '\0';
  if (tid == GetCurrentThreadId()) return false;
  HANDLE t = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, tid);
  if (!t) {
    std::snprintf(out, n, "could not be looked at (thread %lu: error %lu)", tid, GetLastError());
    return false;
  }
  // Held only for the copy: nothing that could wait on a lock it holds.
  CONTEXT c{};
  c.ContextFlags = CONTEXT_CONTROL;
  void* stack[kSlots] = {};
  int copied = 0;
  bool got = false;
  if (SuspendThread(t) != static_cast<DWORD>(-1)) {
    if (GetThreadContext(t, &c)) {
      got = true;
      // The used stack, from its top up: one committed region.
      MEMORY_BASIC_INFORMATION mbi{};
      if (c.Esp && VirtualQuery(reinterpret_cast<void*>(c.Esp), &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT &&
          !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD))) {
        const uintptr_t end = reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
        const auto avail = static_cast<int>((end - c.Esp) / sizeof(void*));
        copied = avail < kSlots ? avail : kSlots;
        std::memcpy(stack, reinterpret_cast<void*>(c.Esp), copied * sizeof(void*));
      }
    }
    ResumeThread(t);
  }
  CloseHandle(t);
  if (!got) {
    std::snprintf(out, n, "could not be looked at (thread %lu: its context)", tid);
    return false;
  }
  char where[MAX_PATH + 32];
  describe(reinterpret_cast<void*>(c.Eip), where, sizeof(where));
  size_t k = std::snprintf(out, n, "is at %s (thread %lu); up its stack:", where, tid);
  int shown = 0;
  for (int i = 0; i < copied && shown < kShown && k < n; ++i) {
    MEMORY_BASIC_INFORMATION mbi{};
    HMODULE owner = nullptr;
    if (!stack[i] || !VirtualQuery(stack[i], &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        !(mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) ||
        !GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<LPCSTR>(stack[i]), &owner) ||
        !owner)
      continue;
    describe(stack[i], where, sizeof(where));
    k += std::snprintf(out + k, n - k, "%s %s", shown ? "," : "", where);
    ++shown;
  }
  if (!shown && k < n) std::snprintf(out + k, n - k, " nothing in a module's code");
  return true;
}

}  // namespace re5cc
