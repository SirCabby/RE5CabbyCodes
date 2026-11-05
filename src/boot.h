#pragma once

// The mod's first moment with the game's code in memory. re5dx9.exe is
// Enigma-wrapped: DllMain runs before the wrapper has unpacked anything, so
// nothing that reads the game's code or its import table can run there. The
// game's own first call of Direct3DCreate9 (the mod's export) comes from its
// renderer's start-up, with the image unpacked and the import table filled:
// unpacked() runs there, once, before the real call - the import hooks (the
// crash filter, the input guard), the class records, the game's discovery and
// the tick's hook.
namespace re5cc::boot {

// From DllMain: the exe's own GetProcAddress import - the one Enigma's loader
// resolves the game's imports through as it unpacks - re-pointed at a hook that
// hands the game the mod's Direct3DCreate9, DirectInput8Create and crash filter
// in place of the real ones (Enigma loads the system's d3d9.dll by itself, so
// the game's Direct3DCreate9 would otherwise never reach the mod's d3d9.dll).
bool install_loader_hook();
void remove_loader_hook();

void unpacked();
bool done();           // unpacked() has run
const char* status();  // what the panel says while the mod cannot see the game

}  // namespace re5cc::boot
