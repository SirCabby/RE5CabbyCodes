// RE5CabbyCodes - proxy entry point.
//
// We ship as d3d9.dll (see proxy.cpp for why). re5dx9.exe imports it, so
// DllMain runs as the program loads - before Enigma's wrapper has unpacked any
// of the game's code. DllMain therefore only loads the real d3d9.dll, the
// crash logger, the ini and the locks, hooks the exe's own GetProcAddress
// import (Enigma's loader resolves the game's imports through it - boot.cpp)
// and starts a thread that reports until the first tick; everything that needs
// the game's code waits for the game's first call of Direct3DCreate9.

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "boot.h"
#include "cheats.h"
#include "config.h"
#include "crash.h"
#include "dispatch.h"
#include "input.h"
#include "inventory.h"
#include "itembox.h"
#include "log.h"
#include "menus.h"
#include "overlay.h"
#include "proxy.h"
#include "version.h"

namespace {

HMODULE g_self = nullptr;
char g_dir[MAX_PATH] = {};

// Directory this DLL was loaded from, with a trailing separator.
void resolve_own_dir() {
  char path[MAX_PATH] = {};
  GetModuleFileNameA(g_self, path, MAX_PATH);
  char* slash = std::strrchr(path, '\\');
  if (!slash) slash = std::strrchr(path, '/');
  if (slash) {
    size_t len = static_cast<size_t>(slash - path) + 1;
    if (len < sizeof(g_dir)) {
      std::memcpy(g_dir, path, len);
      g_dir[len] = '\0';
    }
  }
}

// A window's text without sending it a message: GetWindowText asks the
// window's thread for it and waits, and a thread that is not pumping would hold
// the mod thread for ever. InternalGetWindowText reads the stored text.
void window_text(HWND h, char* out, int n) {
  wchar_t w[256] = {};
  InternalGetWindowText(h, w, 256);
  if (!WideCharToMultiByte(CP_ACP, 0, w, -1, out, n, nullptr, nullptr)) out[0] = '\0';
  out[n - 1] = '\0';
}

// "class 'title'" for each of this process's top-level windows - an error box
// from the game says here what it says on screen.
struct WindowList {
  char text[512];
  size_t len;
};

BOOL CALLBACK list_window(HWND hwnd, LPARAM lp) {
  auto* w = reinterpret_cast<WindowList*>(lp);
  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd) || w->len >= sizeof(w->text) - 1) return TRUE;
  char cls[64] = {}, title[96] = {};
  GetClassNameA(hwnd, cls, sizeof(cls));
  window_text(hwnd, title, sizeof(title));
  w->len += std::snprintf(w->text + w->len, sizeof(w->text) - w->len, "%s[%s '%s']", w->len ? "; " : "", cls, title);
  return TRUE;
}

DWORD g_main_thread = 0;  // DllMain's: the exe's own imports load on its primary thread

// Until the first tick, say every 5 s what is on screen and where the game's main
// thread is: a game that never reaches its frame loop is never a log that just stops.
// The line does not wait on the log's lock for long - a thread stuck while it holds
// it would keep this one from saying so.
DWORD WINAPI mod_thread(LPVOID) {
  using namespace re5cc;
  HANDLE ev = dispatch::first_tick_event();
  for (int s = 5; ev && s <= 120 && WaitForSingleObject(ev, 5000) == WAIT_TIMEOUT; s += 5) {
    WindowList w{};
    EnumWindows(list_window, reinterpret_cast<LPARAM>(&w));
    char where[700];
    where_is(g_main_thread, where, sizeof(where));
    logf_nowait(2000, "%d s after start: no tick yet; windows: %s; the main thread %s", s, w.len ? w.text : "none",
                where);
  }
  return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
  using namespace re5cc;
  switch (reason) {
    case DLL_PROCESS_ATTACH: {
      g_self = module;
      g_main_thread = GetCurrentThreadId();
      DisableThreadLibraryCalls(module);
      resolve_own_dir();
      log_init(g_dir);
      logf("RE5CabbyCodes " RE5CC_VERSION " loaded (d3d9.dll proxy), dir=%s pid=%lu thread=%lu", g_dir,
           GetCurrentProcessId(), GetCurrentThreadId());

      if (!proxy::load_original(g_dir)) {
        MessageBoxA(nullptr, proxy::failure(), "RE5CabbyCodes", MB_OK | MB_ICONERROR);
        return FALSE;
      }
      // The adopted D3D9 objects (Windows) point into this image for as long as
      // they live, and so do the game's hooked import slots: never unloaded.
      HMODULE pinned = nullptr;
      GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                         reinterpret_cast<LPCSTR>(&DllMain), &pinned);

      install_crash_logger();
      config::load(g_dir);
      // Before Enigma's loader runs (it runs after every DllMain): the game's
      // imports come to the mod as they are resolved.
      boot::install_loader_hook();
      cheats::init();
      inventory::init();
      itembox::init();
      menus::init();
      dispatch::init();
      if (!config::get().disable_overlay) overlay::init();

      if (HANDLE t = CreateThread(nullptr, 0, mod_thread, nullptr, 0, nullptr)) CloseHandle(t);
      break;
    }
    case DLL_PROCESS_DETACH:
      // `reserved` is non-null when the whole process is ending (not a
      // FreeLibrary). Windows has already ended every other thread by then and
      // this runs under the loader lock, so touching the window, the Direct3D
      // device or ImGui here is unsafe - RE0CabbyCodes hung the game on close
      // that way on Windows (2026-09-25). The OS reclaims all of it: do nothing.
      if (reserved) {
        logf("process exiting - leaving the hooks for the OS to reclaim");
        break;
      }
      // Undo every patch before this image goes away: anything still pointing
      // into the DLL would fault the moment the game touched it.
      logf("unloading - removing hooks");
      dispatch::uninstall();
      boot::remove_loader_hook();
      cheats::remove_hooks();
      input::uninstall();
      overlay::uninstall();
      logf("hooks removed cleanly");
      log_shutdown();
      break;
    default:
      break;
  }
  return TRUE;
}
