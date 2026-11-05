#include "crash.h"

#include <windows.h>

#include <cstdio>
#include <cstring>

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
    default: return "exception";
  }
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

LONG WINAPI on_exception(EXCEPTION_POINTERS* info) {
  if (info && info->ExceptionRecord) {
    void* at = info->ExceptionRecord->ExceptionAddress;
    char where[MAX_PATH + 32];
    describe(at, where, sizeof(where));
    logf("CRASH: %s (0x%08lX) at %s (thread %lu)", code_name(info->ExceptionRecord->ExceptionCode),
         info->ExceptionRecord->ExceptionCode, where, GetCurrentThreadId());
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
      // A few return addresses off the stack, for the ones that land in a module.
      log_stack(reinterpret_cast<void**>(info->ContextRecord->Esp), 128, 8, {});
    }
  }
  if (!g_previous) return EXCEPTION_CONTINUE_SEARCH;
  // A filter we chain to can handle an exception raised on purpose; that is no
  // crash, and the log should say so.
  const LONG r = g_previous(info);
  if (r == EXCEPTION_CONTINUE_EXECUTION) logf("       (the filter we chain to handled it and the program went on - not a crash)");
  return r;
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
  logf("crash logger installed (the game's own filter is chained once it sets one)");
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
  logf("crash: the top-level filter had been replaced (%p) - ours is back on top%s", reinterpret_cast<void*>(cur),
       cur ? ", chaining to it" : "");
}

}  // namespace re5cc
