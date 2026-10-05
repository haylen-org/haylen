// Compiles the Sokol implementations outside Apple platforms with the backend chosen by the build, which defines `SOKOL_D3D11`, `SOKOL_GLCORE`, `SOKOL_GLES3` or `SOKOL_WGPU`.
// The library `sokol_app` leaves the entry point of Windows and Linux processes to the runtime, which exits with the status of the app.
#define SOKOL_IMPL
#if defined(_WIN32) || (defined(__linux__) && !defined(__ANDROID__))
#define SOKOL_NO_ENTRY
#endif
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
