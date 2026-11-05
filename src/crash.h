#pragma once

namespace re5cc {

// Install a last-resort exception filter that records where a fatal fault
// happened, and in which module. Injected mods are the usual suspect for
// crashes at shutdown, so it needs to be possible to tell from the log whether
// the fault is in this DLL or somewhere else entirely.
void install_crash_logger();

// What the game's own SetUnhandledExceptionFilter import points at (image.cpp
// re-points re5dx9.exe's import slot once Enigma has unpacked the image): the
// game then sets the filter we chain to, and ours stays on top.
void* crash_filter_hook();

// What the game's FatalAppExitA and ExitProcess imports point at, for the import
// `name` that resolved to `real` (nullptr for any other): the game's own fatal
// error and every ExitProcess are logged - with the game's code they came through -
// before they go on to `real`.
void* crash_exit_hook(const char* name, void* real);

// Main thread, about once a second: put our filter back on top if something
// replaced it without going through the game's import.
void reassert_crash_filter();

// "module+0xRVA" for an address, for log lines.
void describe_address(void* addr, char* out, unsigned n);

}  // namespace re5cc
