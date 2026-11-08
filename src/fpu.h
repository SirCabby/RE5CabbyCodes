#pragma once

// The x87 and SSE units of the thread the mod's code is running on. The mod is built
// for the x87 (mingw's i686 default): its own floats, ImGui's and the C runtime's
// float formatting all go through the x87 register stack. Its code runs on threads
// that are the game's (the boot inside the game's Direct3DCreate9, the tick, the
// present, the window procedure). A unit left with an exception unmasked (the
// inexact result above all), an exception pending, or its registers in use faults
// the mod's first float. The game's own code, a hook in the system calls the mod
// makes, or a DLL loaded into the process can leave one so.
//
// fpu::Guard gives the mod's code the unit as Windows starts a thread with it and
// puts the thread's own back after, pending exception and all: the game goes on
// with exactly the state it had.

#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace re5cc::fpu {

// FNSTENV's 28 bytes in 32-bit protected mode.
struct Env {
  uint32_t cw;  // control word: exception masks (bits 0-5), precision (8-9), rounding (10-11)
  uint32_t sw;  // status word: exception flags (0-5), ES (7: an unmasked one waits), TOP (11-13)
  uint32_t tw;  // tag word: two bits a register, 11 = empty
  uint32_t ip, op, dp, ds;
};

// Every exception masked, 53-bit precision, round to nearest: what Windows gives a
// thread, and what the game's C runtime keeps.
inline constexpr uint16_t kControl = 0x027F;
// Every SSE exception masked, round to nearest, no flush to zero.
inline constexpr uint32_t kMxcsr = 0x1F80;

// The thread's x87 environment, left as it was. FNSTENV does not wait for a pending
// exception (and masks every exception after it); FNCLEX drops the pending one so that
// FLDENV, which waits, can put it all back.
inline Env env() {
  Env e;
  __asm__ volatile("fnstenv %0\n\tfnclex\n\tfldenv %0" : "=m"(e) : : "memory");
  return e;
}

inline uint32_t mxcsr() {
  uint32_t m;
  __asm__ volatile("stmxcsr %0" : "=m"(m) : : "memory");
  return m;
}

inline int in_use(const Env& e) {
  int n = 0;
  for (int i = 0; i < 8; ++i) n += ((e.tw >> (2 * i)) & 3) != 3;
  return n;
}
inline uint32_t unmasked(const Env& e) { return ~e.cw & 0x3F; }  // the exceptions that would fault
inline bool pending(const Env& e) { return (e.sw & 0x80) != 0; }
inline uint32_t sse_unmasked(uint32_t m) { return ~(m >> 7) & 0x3F; }

// Nothing in it would fault the mod's float code.
inline bool usual(const Env& e, uint32_t m) { return !unmasked(e) && !pending(e) && !in_use(e) && !sse_unmasked(m); }

// The unit as a guard sets it, give or take the flags its own floats leave (masked):
// how the system calls in between can be seen to have changed it.
inline bool as_set(const Env& e, uint32_t m) {
  return (e.cw & 0x0F3F) == (kControl & 0x0F3F) && !pending(e) && !in_use(e) && (m & ~0x3Fu) == (kMxcsr & ~0x3Fu);
}

// "x87 control word 027F, status 0000, tags FFFF; SSE 1F80", and what in it would fault
// the mod's floats: "- unmasked: inexact; pending; 3 registers in use; SSE unmasked: zero divide".
// `m` 0: no SSE word (an exception's context has none).
inline void describe(const Env& e, uint32_t m, char* out, size_t n) {
  static const char* const kNames[6] = {"invalid", "denormal", "zero divide", "overflow", "underflow", "inexact"};
  auto names = [](uint32_t bits, char* to, size_t room) {
    size_t k = 0;
    for (int i = 0; i < 6 && k < room; ++i)
      if (bits & (1u << i)) k += std::snprintf(to + k, room - k, "%s%s", k ? ", " : "", kNames[i]);
  };
  char x87[96] = "", sse[96] = "", what[4][112] = {};
  names(unmasked(e), x87, sizeof(x87));
  names(sse_unmasked(m), sse, sizeof(sse));
  int w = 0;
  if (unmasked(e)) std::snprintf(what[w++], sizeof(what[0]), "unmasked: %s", x87);
  if (pending(e)) std::snprintf(what[w++], sizeof(what[0]), "an exception pending");
  if (in_use(e)) std::snprintf(what[w++], sizeof(what[0]), "%d register(s) in use", in_use(e));
  if (m && sse_unmasked(m)) std::snprintf(what[w++], sizeof(what[0]), "SSE unmasked: %s", sse);
  char word[16] = "";
  if (m) std::snprintf(word, sizeof(word), "; SSE %04X", m);
  std::snprintf(out, n, "x87 control word %04X, status %04X, tags %04X%s%s%s%s%s%s%s%s%s", e.cw & 0xFFFF,
                e.sw & 0xFFFF, e.tw & 0xFFFF, word, w ? " - " : "", what[0], w > 1 ? "; " : "", what[1],
                w > 2 ? "; " : "", what[2], w > 3 ? "; " : "", what[3]);
}

// For a scope: the units as kControl and kMxcsr have them, the register stack empty,
// nothing pending; at its end the thread's own put back, exactly as found.
class Guard {
 public:
  Guard() {
    __asm__ volatile("fnstenv %0\n\tfnclex" : "=m"(found_) : : "memory");
    if ((found_.tw & 0xFFFF) != 0xFFFF) __asm__ volatile("fninit" : : : "memory");  // not the mod's to keep
    __asm__ volatile("fldcw %0" : : "m"(kControl) : "memory");
    __asm__ volatile("stmxcsr %0\n\tldmxcsr %1" : "=m"(found_mxcsr_) : "m"(kMxcsr) : "memory");
  }
  ~Guard() { __asm__ volatile("fnclex\n\tfldenv %0\n\tldmxcsr %1" : : "m"(found_), "m"(found_mxcsr_) : "memory"); }
  Guard(const Guard&) = delete;
  Guard& operator=(const Guard&) = delete;

  const Env& found() const { return found_; }
  uint32_t found_mxcsr() const { return found_mxcsr_; }

 private:
  Env found_;
  uint32_t found_mxcsr_;
};

}  // namespace re5cc::fpu
