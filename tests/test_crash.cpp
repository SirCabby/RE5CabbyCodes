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
// - an exception still logs its kind, the address touched and the stack.
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

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
