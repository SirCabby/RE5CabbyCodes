#pragma once

#include <windows.h>

namespace re5cc::proxy {

// Load the real d3d9.dll - d3d9_orig.dll from `dir` (absolute path, trailing
// separator) when there is one (another d3d9.dll the player had there, renamed),
// else the system's - pin it, and point every thunk at its function of the same
// name, or at a stub that answers 0 for one it lacks. False when the game cannot
// run with it: nothing loadable, or one without Direct3DCreate9 or the other
// function re5dx9.exe imports. The caller shows failure() and refuses to load.
bool load_original(const char* dir);

const char* failure();  // why the load failed, written for the player (a message box)
HMODULE original();     // the real d3d9.dll once it is loaded (null before)
void* real(const char* name);  // the real DLL's function (Direct3DCreate9 for the overlay)

}  // namespace re5cc::proxy
