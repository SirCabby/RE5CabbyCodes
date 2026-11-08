// Offline test of the x87/SSE guard (src/fpu.h) and of the log's line (src/log.cpp,
// built in). A user's first run on Windows (2026-11-05) ended without a word at the
// boot's first float: the boulder's line (flds 0.75f; fstpl; "%.2f"), the first x87
// code the mod runs. On a thread whose unit would fault it - the inexact result
// unmasked, an exception pending, the register stack in use with the invalid
// operation unmasked - the guarded code runs as on a clean unit, and the thread's own
// state is back after it. Build (the mod's own flags; fopen wrapped, to stand in for a
// hook in the system's file calls):
//
//   i686-w64-mingw32-g++ -std=c++20 -O2 -Wall -Wextra -Wno-cast-function-type -DWIN32_LEAN_AND_MEAN -DNOMINMAX
//       -Isrc -static -Wl,--wrap=fopen tests/test_fpu.cpp -o tests/build/test_fpu.exe
//   wine tests/build/test_fpu.exe
//
// Each state runs in a child of its own (the bare unit faults on purpose):
// - the boulder's code on the bare unit: faults in each state that claims to (so the
//   states are what they say), runs in the others;
// - under the guard: runs, on control word 027F, an empty stack, nothing pending and
//   SSE 1F80; the guard read the state as it was; after it the control word, the
//   status (flags, ES, TOP), which registers are in use and the SSE word are back;
// - logf on each state its call can be made in: the line written with 0.75, the
//   caller's unit as it was.
// In this process:
// - fpu::env() reads a pending exception without raising it, and leaves it pending;
// - describe, usual and as_set say what is in a state;
// - a file call that changes the unit: logf says so once, and puts the caller's back;
// - logf_nowait while another thread holds the log's lock: written, and says so;
// - a line longer than logf's 1 KB is written whole.
#include "../src/log.cpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

using re5cc::fpu::Env;

int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// --- a hook in the system's file calls, made up: it leaves the unit changed --------
bool g_dirty_files = false;
const uint16_t kDirtyControl = 0x025F;  // the inexact result unmasked: a float formatted on it faults

}  // namespace

extern "C" FILE* __real_fopen(const char* path, const char* mode);
extern "C" FILE* __wrap_fopen(const char* path, const char* mode) {
  FILE* f = __real_fopen(path, mode);
  // (its flags dropped first: unmasking the inexact result over the flag a format leaves would fault here)
  if (g_dirty_files) __asm__ volatile("fnclex\n\tfldcw %0\n\tfld1" : : "m"(kDirtyControl) : "memory");
  return f;
}

namespace {

// --- the states ---------------------------------------------------------------------
struct Case {
  const char* name;
  bool bare_faults;  // the boulder's code faults on the bare unit
  bool loggable;     // a call of logf with a float can be made on it (its own one push)
};
const Case kCases[] = {
    {"as Windows starts a thread (027F)", false, true},
    {"Delphi's control word (1232: invalid, zero divide, overflow unmasked)", false, true},
    {"the inexact result unmasked (025F)", true, true},
    {"every exception unmasked (0240)", true, true},
    {"the register stack full, invalid unmasked", true, false},
    {"MMX left on (no EMMS), invalid unmasked", true, false},
    {"a zero divide pending", true, false},
    {"7 registers in use, Delphi's control word", true, true},
    {"SSE: Delphi's MXCSR (1900: invalid, zero divide, overflow unmasked)", false, true},
};
constexpr int kCaseCount = sizeof(kCases) / sizeof(kCases[0]);

void set_control(uint16_t cw) { __asm__ volatile("fnclex\n\tfldcw %0" : : "m"(cw) : "memory"); }

void set_state(int c) {
  switch (c) {
    case 0: set_control(0x027F); break;
    case 1: set_control(0x1232); break;
    case 2: set_control(0x025F); break;
    case 3: set_control(0x0240); break;
    case 4:
      set_control(0x027E);
      __asm__ volatile("fld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1" : : : "memory");
      break;
    case 5:
      set_control(0x027E);
      __asm__ volatile(".byte 0x0f, 0x6f, 0xc0" : : : "memory");  // movq %mm0, %mm0
      break;
    case 6: {
      Env e;
      __asm__ volatile("fnstenv %0" : "=m"(e) : : "memory");
      e.cw = 0x027B;  // the zero divide unmasked
      e.sw = 0x0084;  // its flag, and ES: it waits for the next x87 instruction
      e.tw = 0xFFFF;
      __asm__ volatile("fldenv %0" : : "m"(e) : "memory");
      break;
    }
    case 7:
      set_control(0x1232);
      __asm__ volatile("fld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1\n\tfld1" : : : "memory");
      break;
    case 8: {
      set_control(0x027F);
      const uint32_t m = 0x1900;
      __asm__ volatile("ldmxcsr %0" : : "m"(m) : "memory");
      break;
    }
  }
}

// Back to a clean unit (no waiting instruction first: one may be pending).
void reset_unit() {
  const uint16_t cw = 0x027F;
  const uint32_t m = 0x1F80;
  __asm__ volatile("fninit\n\tfldcw %0\n\tldmxcsr %1" : : "m"(cw), "m"(m) : "memory");
}

// Which registers are empty (a bit each): what "in use" comes to. The tag of one in use
// is the processor's to work out again from what it holds.
unsigned empties(uint32_t tw) {
  unsigned m = 0;
  for (int i = 0; i < 8; ++i) m |= (((tw >> (2 * i)) & 3) == 3) << i;
  return m;
}

// The boot's first float, as the mod's code has it: a float's 0.75 loaded and stored as
// a double, then formatted with two decimals (mingw's x87 formatting).
const float kBlow = 0.75f;
volatile float g_blow = 0.75f;

__attribute__((noinline)) void boulder(char* out, size_t n) {
  double d;
  __asm__ volatile("flds %1\n\tfstpl %0" : "=m"(d) : "m"(kBlow) : "memory");
  std::snprintf(out, n, "%.2f", d);
}

// --- the child: one state, one way --------------------------------------------------
LONG WINAPI child_filter(EXCEPTION_POINTERS* e) {
  std::printf("exception=%08lX\n", e->ExceptionRecord->ExceptionCode);
  std::fflush(stdout);
  ExitProcess(3);
}

void print_env(const char* key, const Env& e, uint32_t m) {
  std::printf("%s=%04X %04X %04X %04X\n", key, e.cw & 0xFFFF, e.sw & 0xFFFF, e.tw & 0xFFFF, m);
}

int child(const char* how, int c, const char* folder) {
  SetUnhandledExceptionFilter(child_filter);
  const std::string dir = std::string(folder) + "\\";  // (a trailing backslash would quote the command line's quote)
  if (!std::strcmp(how, "log")) re5cc::log_init(dir.c_str());
  char out[32] = "?";
  set_state(c);
  const Env before = re5cc::fpu::env();
  const uint32_t mb = re5cc::fpu::mxcsr();
  if (!std::strcmp(how, "bare")) {
    boulder(out, sizeof(out));
    reset_unit();
    std::printf("result=%s\n", out);
    return 0;
  }
  Env inside{}, found{}, after{};
  uint32_t mi = 0, mf = 0, ma = 0;
  if (!std::strcmp(how, "guard")) {
    {
      re5cc::fpu::Guard g;
      inside = re5cc::fpu::env();
      mi = re5cc::fpu::mxcsr();
      boulder(out, sizeof(out));
      found = g.found();
      mf = g.found_mxcsr();
    }
    after = re5cc::fpu::env();
    ma = re5cc::fpu::mxcsr();
  } else {
    re5cc::logf("cheats: the volcano's boulder: AUTO answers its Pushes; each up %.2f s", static_cast<double>(g_blow));
    after = re5cc::fpu::env();
    ma = re5cc::fpu::mxcsr();
    std::snprintf(out, sizeof(out), "logged");
  }
  reset_unit();
  std::printf("result=%s\n", out);
  print_env("before", before, mb);
  print_env("inside", inside, mi);
  print_env("found", found, mf);
  print_env("after", after, ma);
  return 0;
}

// --- the parent ---------------------------------------------------------------------
// An exception in this process is a failure (Wine's debugger may let a float one go on).
LONG WINAPI parent_filter(EXCEPTION_POINTERS* e) {
  std::printf("FAIL  an exception in the test itself: %08lX at %p\n", e->ExceptionRecord->ExceptionCode,
              e->ExceptionRecord->ExceptionAddress);
  std::fflush(stdout);
  ExitProcess(2);
}

struct Run {
  bool exited = false;
  std::string out;
};

Run run_child(const char* how, int c, const char* dir) {
  char self[MAX_PATH], cmd[3 * MAX_PATH];
  GetModuleFileNameA(nullptr, self, MAX_PATH);
  std::snprintf(cmd, sizeof(cmd), "\"%s\" child %s %d \"%s\"", self, how, c, dir);
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE rd = nullptr, wr = nullptr;
  Run r;
  if (!CreatePipe(&rd, &wr, &sa, 0)) return r;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOA si{};
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = wr;
  si.hStdError = wr;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};
  if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
    CloseHandle(rd);
    CloseHandle(wr);
    return r;
  }
  CloseHandle(wr);
  char buf[512];
  DWORD got = 0;
  while (ReadFile(rd, buf, sizeof(buf), &got, nullptr) && got) r.out.append(buf, got);
  r.exited = WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(rd);
  return r;
}

std::string field(const std::string& out, const char* key) {
  const std::string k = std::string(key) + "=";
  const size_t at = out.find(k);
  if (at == std::string::npos) return "";
  const size_t end = out.find_first_of("\r\n", at);
  return out.substr(at + k.size(), end == std::string::npos ? std::string::npos : end - at - k.size());
}

struct Words {
  unsigned cw = 0, sw = 0, tw = 0, m = 0;
  bool ok = false;
};
Words words(const std::string& out, const char* key) {
  Words w;
  w.ok = std::sscanf(field(out, key).c_str(), "%x %x %x %x", &w.cw, &w.sw, &w.tw, &w.m) == 4;
  return w;
}

bool same(const Words& a, const Words& b) {
  return a.ok && b.ok && a.cw == b.cw && a.sw == b.sw && empties(a.tw) == empties(b.tw) && a.m == b.m;
}

std::string read_file(const std::string& path) {
  std::string s;
  if (FILE* f = __real_fopen(path.c_str(), "rb")) {
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    std::fclose(f);
  }
  return s;
}

int count(const std::string& s, const char* what) {
  int n = 0;
  for (size_t at = s.find(what); at != std::string::npos; at = s.find(what, at + 1)) ++n;
  return n;
}

DWORD WINAPI hold_lock(LPVOID ev) {
  EnterCriticalSection(&re5cc::g_lock);
  SetEvent(static_cast<HANDLE*>(ev)[0]);
  WaitForSingleObject(static_cast<HANDLE*>(ev)[1], INFINITE);
  LeaveCriticalSection(&re5cc::g_lock);
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc >= 5 && !std::strcmp(argv[1], "child")) return child(argv[2], std::atoi(argv[3]), argv[4]);
  using namespace re5cc;
  SetUnhandledExceptionFilter(parent_filter);

  char tmp[MAX_PATH];
  GetTempPathA(MAX_PATH, tmp);
  const std::string dir = std::string(tmp) + "re5cc_test_fpu_" + std::to_string(GetCurrentProcessId()) + "\\";
  CreateDirectoryA(dir.c_str(), nullptr);
  const std::string child_folder = dir + "child", child_dir = child_folder + "\\";
  CreateDirectoryA(child_folder.c_str(), nullptr);

  // Each state, in children.
  for (int c = 0; c < kCaseCount; ++c) {
    const Case& k = kCases[c];
    std::printf("-- %s\n", k.name);
    const Run bare = run_child("bare", c, child_folder.c_str());
    char what[200];
    if (k.bare_faults) {
      std::snprintf(what, sizeof(what), "bare: the boulder's code faults (exception %s)",
                    field(bare.out, "exception").c_str());
      check(bare.exited && !field(bare.out, "exception").empty() && field(bare.out, "result").empty(), what);
    } else {
      check(bare.exited && field(bare.out, "result") == "0.75", "bare: the boulder's code runs");
    }
    const Run g = run_child("guard", c, child_folder.c_str());
    check(g.exited && field(g.out, "result") == "0.75" && field(g.out, "exception").empty(),
          "guarded: the boulder's code runs, \"0.75\"");
    const Words before = words(g.out, "before"), inside = words(g.out, "inside"), found = words(g.out, "found"),
                after = words(g.out, "after");
    check(inside.ok && inside.cw == 0x027F && empties(inside.tw) == 0xFF && !(inside.sw & 0x80FF) && inside.m == 0x1F80,
          "... on control word 027F, the stack empty, no flag (none pending), SSE 1F80");
    check(same(found, before), "... the guard read the state as it was");
    check(same(after, before), "... and after it the state is back: control word, status, registers in use, SSE");
    if (k.loggable) {
      const Run l = run_child("log", c, child_folder.c_str());
      const std::string text = read_file(child_dir + "RE5CabbyCodes.log");
      check(l.exited && field(l.out, "result") == "logged" && field(l.out, "exception").empty() &&
                count(text, "each up 0.75 s") == 1,
            "logf: the line written, \"each up 0.75 s\"");
      check(same(words(l.out, "after"), words(l.out, "before")), "... the caller's unit as it was");
    }
  }

  // In this process.
  std::printf("-- in this process\n");
  set_state(6);
  const fpu::Env pend = fpu::env();
  const fpu::Env pend2 = fpu::env();
  reset_unit();
  check(fpu::pending(pend) && (pend.sw & 0x3F) == 0x04 && (pend.cw & 0x3F) == 0x3B && pend2.sw == pend.sw,
        "env() reads a pending zero divide without raising it, and leaves it pending");

  char d[300];
  fpu::Env e{0x025F, 0, 0xFFFF, 0, 0, 0, 0};
  fpu::describe(e, 0x1F80, d, sizeof(d));
  check(std::string(d) == "x87 control word 025F, status 0000, tags FFFF; SSE 1F80 - unmasked: inexact",
        "describe: an unmasked exception by its name");
  e = {0x1232, 0x0084, 0x3FFF, 0, 0, 0, 0};
  fpu::describe(e, 0x1900, d, sizeof(d));
  check(std::string(d) == "x87 control word 1232, status 0084, tags 3FFF; SSE 1900 - unmasked: invalid, zero divide, "
                          "overflow; an exception pending; 1 register(s) in use; SSE unmasked: invalid, zero divide, "
                          "overflow",
        "describe: Delphi's words, an exception pending, a register in use");
  e = {0x027E, 0x82C1, 0x0000, 0, 0, 0, 0};
  fpu::describe(e, 0, d, sizeof(d));
  check(std::string(d) == "x87 control word 027E, status 82C1, tags 0000 - unmasked: invalid; an exception pending; "
                          "8 register(s) in use",
        "describe: no SSE word (an exception's context)");
  check(fpu::usual({0x027F, 0x0020, 0xFFFF, 0, 0, 0, 0}, 0x1FA0) && !fpu::usual({0x1232, 0, 0xFFFF, 0, 0, 0, 0}, 0x1F80) &&
            !fpu::usual({0x027F, 0, 0xFFFF, 0, 0, 0, 0}, 0x1900),
        "usual: masked flags left are usual, an unmasked exception (x87 or SSE) is not");
  check(fpu::as_set({0x027F, 0x0020, 0xFFFF, 0, 0, 0, 0}, 0x1FA0) && !fpu::as_set({0x007F, 0, 0xFFFF, 0, 0, 0, 0}, 0x1F80) &&
            !fpu::as_set({0x027F, 0, 0xFFFC, 0, 0, 0, 0}, 0x1F80) && !fpu::as_set({0x027F, 0, 0xFFFF, 0, 0, 0, 0}, 0x9F80),
        "as_set: the guard's state with its own flags; not another precision, a register in use, flush to zero");

  log_init(dir.c_str());
  const std::string path = dir + "RE5CabbyCodes.log";
  const fpu::Env clean_before = fpu::env();
  const uint32_t clean_mx = fpu::mxcsr();
  g_dirty_files = true;
  logf("first line, a file call changing the unit under it: %.2f", static_cast<double>(g_blow));
  logf("second line, the same");
  g_dirty_files = false;
  const fpu::Env clean_after = fpu::env();
  const uint32_t after_mx = fpu::mxcsr();
  reset_unit();
  std::string text = read_file(path);
  check(count(text, "log: the thread's x87/SSE unit changed across a write of this log (to x87 control word 025F") == 1 &&
            count(text, "unmasked: inexact; 1 register(s) in use") == 1,
        "a file call changing the unit: logf says so, once, with what it changed to");
  check(count(text, "first line, a file call changing the unit under it: 0.75") == 1 && count(text, "second line") == 1,
        "... both lines written (the float formatted before the file was opened)");
  check(clean_after.cw == clean_before.cw && clean_after.sw == clean_before.sw &&
            empties(clean_after.tw) == empties(clean_before.tw) && after_mx == clean_mx,
        "... and the caller's unit put back");

  HANDLE evs[2] = {CreateEventA(nullptr, TRUE, FALSE, nullptr), CreateEventA(nullptr, TRUE, FALSE, nullptr)};
  DWORD holder = 0;
  HANDLE t = CreateThread(nullptr, 0, hold_lock, evs, 0, &holder);
  WaitForSingleObject(evs[0], 5000);
  const DWORD t0 = GetTickCount();
  logf_nowait(100, "5 s after start: no tick yet (the lock held elsewhere)");
  const DWORD waited = GetTickCount() - t0;
  SetEvent(evs[1]);
  WaitForSingleObject(t, 5000);
  logf_nowait(100, "10 s after start: no tick yet (the lock free)");
  text = read_file(path);
  char held[160];
  std::snprintf(held, sizeof(held),
                "(written without the log's lock: thread %lu has held it for over 100 ms) 5 s after start: no tick yet "
                "(the lock held elsewhere)",
                holder);
  check(count(text, held) == 1 && waited >= 90 && waited < 2000,
        "logf_nowait, the lock held by another thread: written after the wait, and says whose");
  check(count(text, "] 10 s after start: no tick yet (the lock free)") == 1, "... with the lock free: a plain line");

  std::string long_text(3000, 'x');
  logf("long: %s.", long_text.c_str());
  text = read_file(path);
  check(count(text, ("long: " + long_text + ".\r\n").c_str()) == 1, "a line of 3 KB written whole");

  std::printf(g_fail ? "%d FAILED\n" : "all passed (0 failed)\n", g_fail);
  return g_fail ? 1 : 0;
}
