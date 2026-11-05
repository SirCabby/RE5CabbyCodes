// X(index, exported name, bytes of arguments) for every function d3d9.dll exports - Windows' and
// DXVK's (Proton's d3d9.dll) together. All are __stdcall: a stub for one the real DLL lacks clears
// its arguments off the stack (proxy.cpp). Direct3DCreate9 is the mod's own (overlay_dx9.cpp), so
// it is not here: d3d9.def names it.
#define PROXY_EXPORTS(X)                               \
  X(0, "D3DPERF_BeginEvent", 8)                        \
  X(1, "D3DPERF_EndEvent", 0)                          \
  X(2, "D3DPERF_GetStatus", 0)                         \
  X(3, "D3DPERF_QueryRepeatFrame", 0)                  \
  X(4, "D3DPERF_SetMarker", 8)                         \
  X(5, "D3DPERF_SetOptions", 4)                        \
  X(6, "D3DPERF_SetRegion", 8)                         \
  X(7, "DebugSetLevel", 0)                             \
  X(8, "DebugSetMute", 0)                              \
  X(9, "Direct3D9EnableMaximizedWindowedModeShim", 4)  \
  X(10, "Direct3DCreate9Ex", 8)                        \
  X(11, "Direct3DCreate9On12", 12)                     \
  X(12, "Direct3DCreate9On12Ex", 16)                   \
  X(13, "Direct3DShaderValidatorCreate9", 0)           \
  X(14, "PSGPError", 12)                               \
  X(15, "PSGPSampleTexture", 20)

#define PROXY_EXPORT_COUNT 16
