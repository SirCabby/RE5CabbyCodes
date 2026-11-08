#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "fpu.h"

namespace re5cc {
namespace {

char             g_path[MAX_PATH] = {};
CRITICAL_SECTION g_lock;
bool             g_ready = false;
volatile LONG    g_unit_reported = 0;

// "[hh:mm:ss.mmm] " and `note`.
int stamp(char* out, size_t room, const char* note) {
  SYSTEMTIME t;
  GetLocalTime(&t);
  return std::snprintf(out, room, "[%02u:%02u:%02u.%03u] %s", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, note);
}

// The stamp and the message: in `small` when it fits, else in a buffer of its own
// (which the caller frees).
char* format(char* small, size_t room, const char* note, const char* fmt, va_list args) {
  const int head = stamp(small, room, note);
  if (head < 0 || static_cast<size_t>(head) >= room) return small;
  va_list copy;
  va_copy(copy, args);
  const int body = std::vsnprintf(small + head, room - head, fmt, copy);
  va_end(copy);
  if (body < 0 || static_cast<size_t>(head) + body < room) return small;
  char* big = static_cast<char*>(std::malloc(static_cast<size_t>(head) + body + 1));
  if (!big) return small;  // the line as far as it fits
  std::memcpy(big, small, head);
  std::vsnprintf(big + head, static_cast<size_t>(body) + 1, fmt, args);
  return big;
}

void append(const char* line) {
  if (FILE* f = std::fopen(g_path, "a")) {
    std::fputs(line, f);
    std::fputc('\n', f);
    std::fclose(f);  // flush-per-line: a crash must not lose the last message
  }
}

// After the file calls, the unit should still be as the line's guard set it. If it is
// not, a hook in the system's file calls changed it: said once (the guard puts the
// caller's back all the same).
void check_unit() {
  const fpu::Env e = fpu::env();
  const uint32_t m = fpu::mxcsr();
  if (fpu::as_set(e, m) || InterlockedExchange(&g_unit_reported, 1)) return;
  fpu::Guard clean;
  char now[256], line[640];
  fpu::describe(e, m, now, sizeof(now));
  const int head = stamp(line, sizeof(line), "");
  std::snprintf(line + head, sizeof(line) - head,
                "log: the thread's x87/SSE unit changed across a write of this log (to %s; the line was written with "
                "control word %04X, no register in use, SSE %04X): something in the system's file calls changes it - "
                "the mod puts back what its caller had",
                now, fpu::kControl, fpu::kMxcsr);
  append(line);
}

bool take_lock(DWORD wait_ms) {
  const DWORD start = GetTickCount();
  while (!TryEnterCriticalSection(&g_lock)) {
    if (GetTickCount() - start >= wait_ms) return false;
    Sleep(10);
  }
  return true;
}

}  // namespace

void log_init(const char* dir) {
  if (g_ready) return;
  InitializeCriticalSection(&g_lock);
  g_ready = true;

  snprintf(g_path, sizeof(g_path), "%sRE5CabbyCodes.log", dir);

  // Truncate on each launch so the log always describes the current run - but
  // keep the run before it, because the first thing that happens after a crash
  // is a relaunch, and that would otherwise wipe the only record of it.
  char previous[MAX_PATH];
  snprintf(previous, sizeof(previous), "%sRE5CabbyCodes.prev.log", dir);
  MoveFileExA(g_path, previous, MOVEFILE_REPLACE_EXISTING);
  if (FILE* f = fopen(g_path, "w")) fclose(f);
}

void log_shutdown() {
  if (!g_ready) return;
  g_ready = false;
  DeleteCriticalSection(&g_lock);
}

// The line is formatted before the file is opened, on the unit the guard sets - the
// C runtime's float formatting is x87 code, whatever the caller's thread had - and the
// caller's unit is put back after the file calls, whatever they did to it.
void logf(const char* fmt, ...) {
  if (!g_ready || !g_path[0]) return;
  fpu::Guard keep;
  char small[1024];
  va_list args;
  va_start(args, fmt);
  char* line = format(small, sizeof(small), "", fmt, args);
  va_end(args);
  EnterCriticalSection(&g_lock);
  append(line);
  check_unit();
  LeaveCriticalSection(&g_lock);
  if (line != small) std::free(line);
}

void logf_nowait(unsigned long wait_ms, const char* fmt, ...) {
  if (!g_ready || !g_path[0]) return;
  fpu::Guard keep;
  const bool locked = take_lock(wait_ms);
  char note[96] = "";
  if (!locked)
    std::snprintf(note, sizeof(note), "(written without the log's lock: thread %lu has held it for over %lu ms) ",
                  static_cast<unsigned long>(reinterpret_cast<uintptr_t>(g_lock.OwningThread)), wait_ms);
  char small[1024];
  va_list args;
  va_start(args, fmt);
  char* line = format(small, sizeof(small), note, fmt, args);
  va_end(args);
  append(line);
  if (locked) LeaveCriticalSection(&g_lock);
  if (line != small) std::free(line);
}

}  // namespace re5cc
