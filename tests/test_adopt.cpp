// Offline test of how the mod knows the game's window on Windows (src/adopt.cpp, built in): only a device made for
// the game's own window is adopted (given vtables of its own, the path the Steam overlay needs), so the window check
// decides whether the panel draws on Windows at all. Until 2026-11-05 it looked for RE4OG's class name, "RE4HDWnd",
// and matched no window of re5dx9.exe's, whose class is "MTFramework" (every log's "overlay ready" line). Build:
//
//   i686-w64-mingw32-g++ -std=c++20 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -Isrc -static tests/test_adopt.cpp
//       -luser32 -o tests/build/test_adopt.exe
//   wine tests/build/test_adopt.exe
//
// - a window of the game's class ("MTFramework") is the game's;
// - a window of RE4's class, of a tool's class and none at all are not;
// - with RE5CC_ADOPT set (the Windows path under Wine) adoption is on.
#include "../src/adopt.cpp"

#include <cstdarg>
#include <cstdio>

namespace re5cc {
void logf(const char*, ...) {}
}  // namespace re5cc

namespace {
int g_fail = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
  if (!ok) ++g_fail;
}

// A message-only window of a class of this name (no display needed).
HWND window_of(const char* cls) {
  WNDCLASSEXW wc{};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = DefWindowProcW;
  wc.hInstance = GetModuleHandleW(nullptr);
  wchar_t name[64] = {};
  MultiByteToWideChar(CP_ACP, 0, cls, -1, name, 64);
  wc.lpszClassName = name;
  RegisterClassExW(&wc);
  return CreateWindowExW(0, name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
}
}  // namespace

int main() {
  SetEnvironmentVariableA("RE5CC_ADOPT", "1");
  re5cc::adopt::init();
  check(re5cc::adopt::enabled(), "RE5CC_ADOPT set: the Windows path (adoption) is on");
  const HWND game = window_of("MTFramework"), re4 = window_of("RE4HDWnd"), tool = window_of("ReShadeWindow");
  check(game && re4 && tool, "three windows made (message-only)");
  check(re5cc::adopt::game_window(game), "a window of the game's class, MTFramework, is the game's");
  check(!re5cc::adopt::game_window(re4), "a window of RE4's class is not");
  check(!re5cc::adopt::game_window(tool), "a window of a tool's class is not");
  check(!re5cc::adopt::game_window(nullptr), "no window is not");
  std::printf("%s (%d failed)\n", g_fail ? "FAILED" : "all passed", g_fail);
  return g_fail ? 1 : 0;
}
