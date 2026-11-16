// Offline test of how the mod logs the game's own ways of ending (src/crash.cpp,
// built in): its fatal error through FatalAppExitA, ExitProcess, and an exception
// reaching the top-level filter. The game is made up: a "fatal error" that formats
// its text into a 1 KB buffer on its stack and hands it to its handler, as the
// game's FUN_006CFEF0 does. Build (the mod's own flags; no object of the mod linked):
//
//   i686-w64-mingw32-g++ -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type -DWIN32_LEAN_AND_MEAN -DNOMINMAX
//       -Isrc -static tests/test_crash.cpp -o tests/build/test_crash.exe
//   wine tests/build/test_crash.exe
//
// - only FatalAppExitA and ExitProcess are taken, and never without the real one;
// - the fatal error's text is logged (its newline dropped), with the code it came
//   through - past the 1 KB of text on the stack, up to the function that failed -
//   and the real FatalAppExitA then gets the game's arguments;
// - ExitProcess(0), a quit, is one line; any other code is logged with where it came
//   from; the real ExitProcess gets the code;
// - an exception still logs its kind, the address touched and the stack; a
//   floating-point one its x87 state too;
// - as Windows first reports it (before any handler, which may end the program first,
//   as on a user's first Windows run, 2026-11-05): a fault in this program's code
//   ("the mod's"), or in another module with this program's return address near the
//   top of the stack, is logged; one with nothing of it near there, and anything not
//   an error (a breakpoint, a debugger's message, a thread's name, a C++ throw), is
//   not; four a run at most; nothing handled;
// - where_is: another thread's place and its stack's code addresses, read while it
//   is held; not this thread's, nor one that is not there;
// - a filter that replaced ours: named by its module, ours back on top, chained to it.
// - the chain (2026-11-10, after a crash report of RE4OGCabbyCodes' - the same filter,
//   chaining and reassert): the filter called as UnhandledExceptionFilter calls the
//   top-level one, with a made-up access violation: installed and the game's filter set
//   through the hook, ours stays on top; a crash is one dump with every register and
//   the top of the stack raw, passed on to the filter named; the same exception given
//   to ours again is a line; a filter set over ours that chains to the one it replaced
//   (ours) - reassert chaining to it - is stopped as it hands the same exception back
//   (before, ours went round until the stack ran out); a "previous filter" that is no
//   code is not chained to; two threads each get their own dump; a new exception raised
//   inside the filter ours passed one on to is passed on as well, but not past three
//   deep; SetUnhandledExceptionFilter made to do nothing (a `ret 4` over its first
//   bytes, another crash handler keeping the slot): said once, nothing taken for a
//   filter, no line each second, the game's call and install not claimed to put ours
//   on top.
#include "../src/crash.cpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_lines;
SRWLOCK g_lines_lock = SRWLOCK_INIT;  // the chain's two threads log at once
bool g_verbose = false;
}  // namespace

namespace re5cc {
void logf(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  AcquireSRWLockExclusive(&g_lines_lock);
  g_lines.emplace_back(buf);
  if (g_verbose) std::printf("      log: %s\n", buf);
  ReleaseSRWLockExclusive(&g_lines_lock);
}
namespace image {
// This program's own code stands for the game's.
mem::Range text() {
  const auto base = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr));
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
  const IMAGE_SECTION_HEADER* s = IMAGE_FIRST_SECTION(nt);
  for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++s)
    if (!std::strncmp(reinterpret_cast<const char*>(s->Name), ".text", 8))
      return {base + s->VirtualAddress, base + s->VirtualAddress + s->Misc.VirtualSize};
  return {};
}
}  // namespace image
}  // namespace re5cc

namespace {
using namespace re5cc;

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

bool logged(const char* text) {
  for (const std::string& l : g_lines)
    if (l.find(text) != std::string::npos) return true;
  return false;
}

// "test_crash.exe+0xRVA" for an address, as the log writes it.
std::string where(void* a) {
  char out[MAX_PATH + 32];
  describe(a, out, sizeof(out));
  return out;
}

int stack_lines() {
  int n = 0;
  for (const std::string& l : g_lines) n += l.find("stack[") != std::string::npos;
  return n;
}

// The real functions, stood in for.
UINT g_fatal_action = 99, g_exit_code = 99;
LPCSTR g_fatal_text = nullptr;
void WINAPI real_fatal(UINT action, LPCSTR text) {
  g_fatal_action = action;
  g_fatal_text = text;
}
void WINAPI real_exit(UINT code) { g_exit_code = code; }

// The made-up game: its import slots, its handler, its fatal error and a load that fails.
FatalAppExitFn g_slot_fatal = nullptr;
ExitProcessFn g_slot_exit = nullptr;
void* g_back_in_load = nullptr;  // the fatal error's return into the load that failed

__attribute__((noinline)) void handler(const char* text) { g_slot_fatal(0, text); }

__attribute__((noinline)) void game_fatal(const char* fmt, ...) {
  g_back_in_load = __builtin_return_address(0);
  char buf[0x400];
  std::memset(buf, 'x', sizeof(buf));  // text all the way, as a long message would leave
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, 0x200, fmt, ap);
  va_end(ap);
  handler(buf);
  asm volatile("" ::"r"(buf) : "memory");
}

__attribute__((noinline)) void game_load(const char* path) {
  game_fatal("Failed open file. %s %d\n", path, 183);
  asm volatile("" ::: "memory");
}

__attribute__((noinline)) void game_abort() {
  g_slot_exit(255);
  asm volatile("" ::: "memory");
}

int touch_me() { return 1; }

// A thread of this program's, parked in a wait.
HANDLE g_park = nullptr;
__attribute__((noinline)) DWORD WINAPI parked(LPVOID) {
  WaitForSingleObject(g_park, INFINITE);
  asm volatile("" ::: "memory");
  return 0;
}

LONG WINAPI other_filter(EXCEPTION_POINTERS*) { return EXCEPTION_CONTINUE_SEARCH; }

// A first report of `code` at `at`, the stack `stack`, as Windows hands it to the
// vectored handler: the lines it logged.
std::vector<std::string> first_chance(DWORD code, void* at, void** stack, CONTEXT* ctx = nullptr) {
  g_lines.clear();
  EXCEPTION_RECORD rec{};
  rec.ExceptionCode = code;
  rec.ExceptionAddress = at;
  if (code == EXCEPTION_ACCESS_VIOLATION) {
    rec.NumberParameters = 2;
    rec.ExceptionInformation[1] = 0x10;
  }
  CONTEXT own{};
  if (!ctx) ctx = &own;
  ctx->Esp = static_cast<DWORD>(reinterpret_cast<uintptr_t>(stack));
  EXCEPTION_POINTERS ptrs{&rec, ctx};
  const LONG r = on_first_chance(&ptrs);
  if (r != EXCEPTION_CONTINUE_SEARCH) g_lines.emplace_back("(handled!)");
  return g_lines;
}

// --- The chain (2026-11-10): the filter called as UnhandledExceptionFilter calls the
// top-level one, beside filters that chain back to ours and an API made inert.

using Filter = LPTOP_LEVEL_EXCEPTION_FILTER;

size_t mark() {
  AcquireSRWLockShared(&g_lines_lock);
  const size_t n = g_lines.size();
  ReleaseSRWLockShared(&g_lines_lock);
  return n;
}

size_t count(const char* needle, size_t from) {
  size_t n = 0;
  AcquireSRWLockShared(&g_lines_lock);
  for (size_t i = from; i < g_lines.size(); ++i) n += g_lines[i].find(needle) != std::string::npos;
  ReleaseSRWLockShared(&g_lines_lock);
  return n;
}

// The filter on top now, the slot left as it was.
Filter top() {
  const Filter t = SetUnhandledExceptionFilter(nullptr);
  SetUnhandledExceptionFilter(t);
  return t;
}

// A made-up access violation: reading address 0 at `at`, with a stack of its own.
struct Fake {
  EXCEPTION_RECORD rec;
  CONTEXT ctx;
  EXCEPTION_POINTERS ptrs;
  uint32_t stack[160];
};
void make(Fake* f, void* at, uint32_t edi) {
  std::memset(f, 0, sizeof(*f));
  f->rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  f->rec.ExceptionAddress = at;
  f->rec.NumberParameters = 2;
  for (int i = 0; i < 160; ++i) f->stack[i] = 0x01000000u + i;
  f->stack[3] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(&make));  // a return address in this program
  f->ctx.ContextFlags = CONTEXT_FULL;
  f->ctx.Eip = static_cast<DWORD>(reinterpret_cast<uintptr_t>(at));
  f->ctx.Esp = static_cast<DWORD>(reinterpret_cast<uintptr_t>(f->stack));
  f->ctx.Eax = 0x10;
  f->ctx.Ecx = 4;
  f->ctx.Esi = 0;
  f->ctx.Edi = edi;
  f->ptrs.ExceptionRecord = &f->rec;
  f->ptrs.ContextRecord = &f->ctx;
}

// The game's own filter, set through its import (the hook).
int g_game_calls = 0;
LONG WINAPI game_filter(EXCEPTION_POINTERS*) {
  ++g_game_calls;
  return EXCEPTION_CONTINUE_SEARCH;
}

// A filter set over ours with the real API, that passes every exception on to the
// one it replaced (as many crash loggers do).
Filter g_x_prev = nullptr;
int g_x_calls = 0;
LONG WINAPI x_filter(EXCEPTION_POINTERS* p) {
  ++g_x_calls;
  return g_x_prev ? g_x_prev(p) : EXCEPTION_CONTINUE_SEARCH;
}

// For the two threads: the first, inside it, waits for the second's crash.
HANDLE g_inside_x = nullptr, g_second_done = nullptr;
DWORD g_waiting_thread = 0;
LONG WINAPI x_waiting_filter(EXCEPTION_POINTERS* p) {
  ++g_x_calls;
  if (GetCurrentThreadId() == g_waiting_thread) {
    SetEvent(g_inside_x);
    WaitForSingleObject(g_second_done, 5000);
  }
  return g_x_prev ? g_x_prev(p) : EXCEPTION_CONTINUE_SEARCH;
}

// A filter that raises a new exception of its own while it handles one - a packer's
// handler may, on purpose - which reaches the top-level filter (ours), and handles
// that one itself (goes on); the first it lets go.
Filter g_top = nullptr;
Fake g_nested;
int g_y_calls = 0;
LONG g_y_nested_r = -9;
LONG WINAPI y_filter(EXCEPTION_POINTERS* p) {
  if (++g_y_calls == 1) {
    g_y_nested_r = g_top(&g_nested.ptrs);
    return EXCEPTION_CONTINUE_SEARCH;
  }
  return p == &g_nested.ptrs ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

// A filter that raises a new exception every time it runs (one that crashes in
// itself, again and again).
Fake g_runaway[8];
int g_z_calls = 0;
LONG WINAPI z_filter(EXCEPTION_POINTERS*) {
  const int k = g_z_calls++;
  return k < 8 ? g_top(&g_runaway[k].ptrs) : EXCEPTION_CONTINUE_SEARCH;
}

struct Fire {
  Filter f;
  Fake* fake;
  LONG r;
};
DWORD WINAPI fire_thread(void* arg) {
  auto* a = static_cast<Fire*>(arg);
  a->r = a->f(&a->fake->ptrs);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  using namespace re5cc;
  g_verbose = argc > 1 && std::string(argv[1]) == "-v";

  check(!crash_exit_hook("Direct3DCreate9", reinterpret_cast<void*>(&real_exit)), "another import: not taken");
  check(!crash_exit_hook("ExitProcess", nullptr) && !crash_exit_hook(nullptr, reinterpret_cast<void*>(&real_exit)),
        "... nor one without the real function");
  g_slot_fatal = reinterpret_cast<FatalAppExitFn>(crash_exit_hook("FatalAppExitA", reinterpret_cast<void*>(&real_fatal)));
  g_slot_exit = reinterpret_cast<ExitProcessFn>(crash_exit_hook("ExitProcess", reinterpret_cast<void*>(&real_exit)));
  check(g_slot_fatal && g_slot_exit, "FatalAppExitA and ExitProcess taken");
  check(crash_exit_hook("FatalAppExitA", reinterpret_cast<void*>(&real_exit)) == reinterpret_cast<void*>(g_slot_fatal),
        "... asked again (the import table's fallback): the same hook, the real one kept");

  // The game's fatal error on a file it could not open.
  g_lines.clear();
  game_load("pawn\\wp\\wp01\\model\\wp0100.mod");
  check(logged("FATAL: the game ends itself with its own error: \"Failed open file. pawn\\wp\\wp01\\model\\wp0100.mod 183\""),
        "the fatal error's text logged, its newline dropped");
  check(g_fatal_action == 0 && g_fatal_text && !std::strncmp(g_fatal_text, "Failed open file.", 17),
        "... then the real FatalAppExitA with the game's arguments");
  check(g_back_in_load && logged(where(g_back_in_load).c_str()),
        "... the code it came through logged past the 1 KB of text, up to the load that failed");
  check(stack_lines() <= 10, "... ten addresses at most");

  g_lines.clear();
  g_fatal_text = nullptr;
  g_slot_fatal(0, nullptr);
  check(logged("\"(none)\"") && !g_fatal_text, "no text: logged as none, the real one still called");

  // ExitProcess: a quit, then a runtime error's.
  g_lines.clear();
  g_slot_exit(0);
  check(g_lines.size() == 1 && logged("the game ends: ExitProcess(0)") && g_exit_code == 0,
        "ExitProcess(0), a quit: one line, then the real one");
  g_lines.clear();
  game_abort();
  check(logged("the game ends: ExitProcess(255) - not a quit's 0") && stack_lines() >= 1 && g_exit_code == 255,
        "ExitProcess(255): logged with where it came from, then the real one");

  // An exception at the top-level filter, as before.
  g_lines.clear();
  void* fake_stack[4] = {nullptr, reinterpret_cast<void*>(&touch_me), nullptr, nullptr};
  EXCEPTION_RECORD rec{};
  rec.ExceptionCode = EXCEPTION_ACCESS_VIOLATION;
  rec.ExceptionAddress = reinterpret_cast<void*>(&touch_me);
  rec.NumberParameters = 2;
  rec.ExceptionInformation[0] = 0;
  rec.ExceptionInformation[1] = 0x10;
  CONTEXT ctx{};
  ctx.Esp = static_cast<DWORD>(reinterpret_cast<uintptr_t>(fake_stack));
  EXCEPTION_POINTERS ptrs{&rec, &ctx};
  const LONG r = on_exception(&ptrs);
  check(r == EXCEPTION_CONTINUE_SEARCH && logged("CRASH: ACCESS_VIOLATION") && logged("reading address 00000010"),
        "an exception: its kind and the address touched");
  check(logged(("stack[001] " + where(reinterpret_cast<void*>(&touch_me))).c_str()), "... and the stack's code addresses");

  // A floating-point exception at the top-level filter: its x87 state.
  g_lines.clear();
  rec.ExceptionCode = EXCEPTION_FLT_STACK_CHECK;
  rec.NumberParameters = 0;
  ctx.ContextFlags = CONTEXT_FLOATING_POINT;
  ctx.FloatSave.ControlWord = 0x027E;
  ctx.FloatSave.StatusWord = 0x82C1;
  ctx.FloatSave.TagWord = 0x0000;
  on_exception(&ptrs);
  check(logged("CRASH: FLT_STACK_CHECK (0xC0000092)") &&
            logged("x87 control word 027E, status 82C1, tags 0000 - unmasked: invalid; an exception pending; 8 "
                   "register(s) in use"),
        "a floating-point exception: named, with its x87 state");

  // As Windows first reports an exception: the mod's own logged, before any handler.
  void* nothing[32] = {};
  void* called_from_here[32] = {nullptr, nullptr, reinterpret_cast<void*>(&touch_me)};
  const std::string here = where(reinterpret_cast<void*>(&touch_me));
  std::vector<std::string> l = first_chance(EXCEPTION_ACCESS_VIOLATION, reinterpret_cast<void*>(&touch_me), nothing);
  check(!l.empty() &&
            l[0].find("exception in the mod's own code, first reported (before any handler, which may yet take it): "
                      "ACCESS_VIOLATION (0xC0000005) at " + here) == 0 &&
            logged("reading address 00000010") &&
            !logged("(handled!)"),
        "first report, a fault in this program's code: its kind, where, what it touched - and not handled");
  void* elsewhere = reinterpret_cast<void*>(&GetTickCount);  // kernel32's
  l = first_chance(EXCEPTION_ACCESS_VIOLATION, elsewhere, called_from_here);
  check(!l.empty() && l[0].find("exception in a call the mod made, first reported") == 0 &&
            l[0].find("at kernel32.dll+0x") != std::string::npos && !logged("(handled!)"),
        "... in another module, this program's return address near the stack's top: a call the mod made");
  l = first_chance(EXCEPTION_ACCESS_VIOLATION, elsewhere, nothing);
  check(l.empty(), "... in another module, nothing of this program's near the stack's top: not logged");
  bool quiet = true;
  for (DWORD code : {0x80000003UL, 0x40010006UL, 0x406D1388UL, 0xE06D7363UL, 0x80000004UL})
    quiet = quiet && first_chance(code, reinterpret_cast<void*>(&touch_me), nothing).empty();
  check(quiet, "... a breakpoint, a debugger's message, a thread's name, a C++ throw, a single step: not logged");
  CONTEXT fctx{};
  fctx.ContextFlags = CONTEXT_FLOATING_POINT;
  fctx.FloatSave.ControlWord = 0x025F;
  fctx.FloatSave.StatusWord = 0x80A0;
  fctx.FloatSave.TagWord = 0x3FFF;
  l = first_chance(EXCEPTION_FLT_INEXACT_RESULT, reinterpret_cast<void*>(&touch_me), nothing, &fctx);
  check(!l.empty() && l[0].find("FLT_INEXACT_RESULT (0xC000008F)") != std::string::npos &&
            logged("x87 control word 025F, status 80A0, tags 3FFF - unmasked: inexact; an exception pending; 1 "
                   "register(s) in use"),
        "... a floating-point fault: named, with its x87 state");
  l = first_chance(EXCEPTION_INT_DIVIDE_BY_ZERO, reinterpret_cast<void*>(&touch_me), nothing);
  const bool fourth = !l.empty();
  l = first_chance(EXCEPTION_INT_DIVIDE_BY_ZERO, reinterpret_cast<void*>(&touch_me), nothing);
  check(fourth && l.empty(), "... four a run, no more");

  // Where another thread is.
  g_park = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  DWORD tid = 0;
  HANDLE t = CreateThread(nullptr, 0, parked, nullptr, 0, &tid);
  Sleep(200);
  char at[700];
  const bool seen = where_is(tid, at, sizeof(at));
  const std::string s = at;
  if (g_verbose) std::printf("      where_is: %s\n", at);
  char thread_no[32];
  std::snprintf(thread_no, sizeof(thread_no), "(thread %lu); up its stack:", tid);
  check(seen && s.find("is at ") == 0 && s.find(thread_no) != std::string::npos &&
            s.find("test_crash.exe+0x") != std::string::npos,
        "where_is: a parked thread's place, and its stack's code addresses up to this program's");
  SetEvent(g_park);
  WaitForSingleObject(t, 5000);
  CloseHandle(t);
  check(!where_is(GetCurrentThreadId(), at, sizeof(at)), "... not this thread's own");
  check(!where_is(0xFFFFFFF0, at, sizeof(at)) && std::string(at).find("could not be looked at") == 0,
        "... nor one that is not there (and says so)");

  // A filter that replaced ours.
  g_lines.clear();
  SetUnhandledExceptionFilter(&other_filter);
  reassert_crash_filter();
  LPTOP_LEVEL_EXCEPTION_FILTER now = SetUnhandledExceptionFilter(nullptr);
  check(logged("the top-level filter had been replaced (") &&
            logged((", " + where(reinterpret_cast<void*>(&other_filter)) + ") - ours is back on top, chaining to it").c_str()) &&
            now == &on_exception && g_previous == &other_filter,
        "a filter that replaced ours: named by its module, ours back on top, chained to it");

  // --- The chain (2026-11-10). Installed, the game's filter set through the hook.
  install_crash_logger();
  const Filter ours = top();
  check(ours == &on_exception, "the chain: ours on top after install");
  const auto hook = reinterpret_cast<SetFilterFn>(crash_filter_hook());
  hook(&game_filter);
  check(top() == ours, "... and after the game sets its filter through the hook");

  Fake f1;
  make(&f1, reinterpret_cast<void*>(&top), 0x0C5D77C);
  size_t m = mark();
  LONG cr = ours(&f1.ptrs);
  check(cr == EXCEPTION_CONTINUE_SEARCH && count("CRASH: ACCESS_VIOLATION", m) == 1 &&
            count("reading address 00000000", m) == 1,
        "a crash: one dump, what it touched, the search goes on");
  check(count("esi=00000000 edi=00C5D77C", m) == 1, "... every register, esi and edi among them");
  check(count("esp+000: 01000000 01000001 01000002", m) == 1 && count("esp+0A0:", m) == 1,
        "... the top of the stack, raw (six rows)");
  check(count(("passed on to the filter at " + where(reinterpret_cast<void*>(&game_filter))).c_str(), m) == 1 &&
            g_game_calls == 1,
        "... passed on to the game's filter, named; it ran once");

  m = mark();
  ours(&f1.ptrs);
  check(count("CRASH: the same ACCESS_VIOLATION", m) == 1 && count("eip=", m) == 0 && g_game_calls == 2,
        "the same exception again: a line, not a second dump; still passed on");

  // A filter set over ours that chains to the one it replaced (ours): reassert puts
  // ours back over it and chains to it - a crash must not go round between the two.
  g_x_prev = SetUnhandledExceptionFilter(&x_filter);
  check(g_x_prev == ours, "a filter set over ours saved ours as the one to chain to");
  m = mark();
  reassert_crash_filter();
  check(top() == ours &&
            count((", " + where(reinterpret_cast<void*>(&x_filter)) + ") - ours is back on top, chaining to it").c_str(),
                  m) == 1,
        "... reassert puts ours back on top, chaining to it (named)");
  Fake f2;
  make(&f2, reinterpret_cast<void*>(&mark), 0x12345678);
  m = mark();
  cr = ours(&f2.ptrs);
  check(g_x_calls == 1 && count("back in our filter with the same exception", m) == 1,
        "... a crash then: the other filter ran once, the way back into ours stopped and logged - no loop");
  check(count("CRASH: ACCESS_VIOLATION", m) == 1 && cr == EXCEPTION_CONTINUE_SEARCH, "... one dump, the search goes on");
  m = mark();
  SetUnhandledExceptionFilter(&x_filter);
  reassert_crash_filter();
  check(count("replaced", m) == 0, "a filter that puts itself back again is not logged again");

  // A "previous filter" that is no code.
  SetUnhandledExceptionFilter(reinterpret_cast<Filter>(static_cast<uintptr_t>(0x3EA)));
  m = mark();
  reassert_crash_filter();
  check(top() == ours && count("which is no code", m) == 1, "a previous filter that is no code: ours back on top, said so");
  Fake f3;
  make(&f3, reinterpret_cast<void*>(&stack_lines), 1);
  const int x_before = g_x_calls;
  ours(&f3.ptrs);  // would jump to 0x3EA if it were chained to
  check(g_x_calls == x_before + 1, "... not chained to: still the last filter that is code");

  // Two threads: one inside the chain, the other crashing meanwhile.
  g_x_prev = SetUnhandledExceptionFilter(&x_waiting_filter);
  reassert_crash_filter();
  g_inside_x = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  g_second_done = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  Fake fa, fb;
  make(&fa, reinterpret_cast<void*>(&fire_thread), 0xAAAA);
  make(&fb, reinterpret_cast<void*>(&x_waiting_filter), 0xBBBB);
  Fire a{ours, &fa, -9}, b{ours, &fb, -9};
  m = mark();
  HANDLE ta = CreateThread(nullptr, 0, fire_thread, &a, CREATE_SUSPENDED, &g_waiting_thread);
  ResumeThread(ta);
  check(WaitForSingleObject(g_inside_x, 5000) == WAIT_OBJECT_0, "two threads: the first is inside the chain");
  HANDLE tb = CreateThread(nullptr, 0, fire_thread, &b, 0, nullptr);
  WaitForSingleObject(tb, 5000);
  check(count("edi=0000BBBB", m) == 1, "... the second's crash meanwhile gets its own dump");
  SetEvent(g_second_done);
  WaitForSingleObject(ta, 5000);
  check(count("edi=0000AAAA", m) == 1 && count("back in our filter", m) == 2 &&
            a.r == EXCEPTION_CONTINUE_SEARCH && b.r == EXCEPTION_CONTINUE_SEARCH,
        "... and the first's; each stopped on its own thread, both searches go on");
  CloseHandle(ta);
  CloseHandle(tb);

  // A new exception raised inside the filter ours passed one on to: passed on too.
  g_top = ours;
  SetUnhandledExceptionFilter(&y_filter);
  reassert_crash_filter();
  Fake f4;
  make(&f4, reinterpret_cast<void*>(&y_filter), 0x4444);
  make(&g_nested, reinterpret_cast<void*>(&z_filter), 0x5555);
  m = mark();
  cr = ours(&f4.ptrs);
  check(g_y_calls == 2 && g_y_nested_r == EXCEPTION_CONTINUE_EXECUTION,
        "a new exception raised in the filter ours passed one on to: passed on as well, its answer back (handled)");
  check(count("another exception, raised while", m) == 1 && count("edi=00004444", m) == 1 &&
            count("edi=00005555", m) == 1 && count("handled it and the program went on", m) == 1 &&
            cr == EXCEPTION_CONTINUE_SEARCH,
        "... said so, each with its dump; the nested one went on, the first one's search goes on");

  // A filter that raises a new exception every time it runs: stopped three deep.
  SetUnhandledExceptionFilter(&z_filter);
  reassert_crash_filter();
  for (int k = 0; k < 8; ++k) make(&g_runaway[k], reinterpret_cast<char*>(&z_filter) + 1 + k, 0x7000 + k);
  Fake f5;
  make(&f5, reinterpret_cast<void*>(&y_filter), 0x6666);
  m = mark();
  cr = ours(&f5.ptrs);
  check(g_z_calls == 3 && count("others on this thread - not passed on", m) == 1 && cr == EXCEPTION_CONTINUE_SEARCH,
        "a filter that raises a new exception every time it runs: stopped three deep, said so, the search goes on");

  // SetUnhandledExceptionFilter made to do nothing, as another crash handler keeping
  // the slot does (re4_tweaks' with a CrashDumps folder): a `ret 4` over its first bytes.
  auto* api = reinterpret_cast<uint8_t*>(&SetUnhandledExceptionFilter);
  uint8_t saved[3];
  std::memcpy(saved, api, 3);
  DWORD old = 0;
  VirtualProtect(api, 3, PAGE_EXECUTE_READWRITE, &old);
  const uint8_t ret4[3] = {0xC2, 0x04, 0x00};
  std::memcpy(api, ret4, 3);
  FlushInstructionCache(GetCurrentProcess(), api, 3);
  m = mark();
  for (int k = 0; k < 5; ++k) reassert_crash_filter();  // five seconds of ticks
  check(count("made to do nothing", m) == 1 && count("replaced", m) == 0,
        "the API made to do nothing: said once; no garbage taken for a filter, no line a second");
  hook(&game_filter);
  check(count("it did not go in", m) == 1 && count("ours stays on top", m) == 0,
        "... the game's call is not claimed to leave ours on top");
  install_crash_logger();
  check(count("installed without its top-level filter", m) == 1, "... install stands aside, and says so");
  std::memcpy(api, saved, 3);
  VirtualProtect(api, 3, old, &old);
  FlushInstructionCache(GetCurrentProcess(), api, 3);
  check(top() == ours, "... the API back as it was: ours is still on top");

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
