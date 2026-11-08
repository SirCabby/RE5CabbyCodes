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
#include "../src/crash.cpp"

#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

namespace {
std::vector<std::string> g_lines;
bool g_verbose = false;
}  // namespace

namespace re5cc {
void logf(const char* fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  g_lines.emplace_back(buf);
  if (g_verbose) std::printf("      log: %s\n", buf);
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

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
