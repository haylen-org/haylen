// The trace hooks let tests see what the renderer asks of `sokol_gfx`, which the dummy backend never draws.
#define SOKOL_IMPL
#define SOKOL_DUMMY_BACKEND
#define SOKOL_TRACE_HOOKS
#include "sokol_gfx.h"
