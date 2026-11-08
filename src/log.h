#pragma once

// Minimal file logger. The mod runs inside a 2009 game under Proton, where a
// debugger is awkward and stdout goes nowhere, so a log file beside the DLL is
// the primary diagnostic channel.
namespace re5cc {

// Called once from DllMain with the directory the DLL was loaded from.
void log_init(const char* dir);
void log_shutdown();

void logf(const char* fmt, ...);

// The same line, for a thread that must not wait on another's: once the log's lock
// has been held elsewhere for `wait_ms`, the line is written without it (and says so).
void logf_nowait(unsigned long wait_ms, const char* fmt, ...);

}  // namespace re5cc
