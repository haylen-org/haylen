#pragma once

// The headers of Clipper2, which the engine includes only through this file. The code generator of MSVC reports the clamp that the inline `CheckPrecisionRange` of Clipper2 keeps after a call that always throws as unreachable code, which `/external:W0` does not cover, and MSVC keeps the state of a warning of its code generator that a function definition starts with, so the warning is off for the definitions of these headers alone.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
#include <clipper2/clipper.h>
#include <clipper2/clipper.triangulation.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
