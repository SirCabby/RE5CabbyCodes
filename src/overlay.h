#pragma once

#include <windows.h>

// The Dear ImGui panel and the machinery that gets it on screen. The game's
// Direct3DCreate9 is the mod's own (it is d3d9.dll), so the renderer is
// captured from the first call; the device's presents come from the game's
// render thread - ImGui lives entirely on that thread, and reads the game only
// through the snapshots the main-thread tick publishes. The window procedure
// (the main thread pumps the window's messages) feeds the same context, under
// its lock.
namespace re5cc::overlay {

void init();       // from DllMain: the lock, the adoption decision (no hooks yet)
void uninstall();

bool ensure_context(HWND hwnd);   // ImGui context + Win32 backend + WndProc, once
HWND window();                    // the game's window, once the overlay has seen it
bool wants_draw();                // the visibility rule for this frame
void draw_frame();                // between ImGui::NewFrame() and ImGui::Render()
void shutdown_imgui();            // Win32 backend + context (renderer already gone)

// One ImGui context; the window procedure feeds it messages and the present
// hook draws from it. Every touch of the context is taken under this lock
// (RE0 learned it the hard way: a frame freed the event queue under a
// WM_MOUSEMOVE that was reading it).
void lock_imgui();
void unlock_imgui();
struct ImGuiLock {
  ImGuiLock() { lock_imgui(); }
  ~ImGuiLock() { unlock_imgui(); }
  ImGuiLock(const ImGuiLock&) = delete;
  ImGuiLock& operator=(const ImGuiLock&) = delete;
};

// What the panel is using this frame, for the input guard (input.cpp).
bool visible();             // the panel is on screen and wants the pointer
bool capturing_mouse();     // the pointer is over the panel
bool capturing_keyboard();  // a panel field has keyboard focus

namespace dx9 {
void uninstall();

// What the log shows about the device.
struct Stats {
  unsigned long present_thread = 0;
  unsigned present_per_s = 0;       // IDirect3DDevice9::Present
  unsigned swap_present_per_s = 0;  // IDirect3DSwapChain9::Present
  unsigned end_scene_per_s = 0;
  unsigned backbuffer_w = 0, backbuffer_h = 0;
  bool windowed = false;
};
Stats stats();
void log_rates();  // main thread, once a second: the counts of the last second
}  // namespace dx9

}  // namespace re5cc::overlay
