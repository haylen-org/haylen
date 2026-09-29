// Apple platforms compile the Sokol implementations as Objective-C because sokol_app talks to AppKit and UIKit.
// sokol_app leaves main to the app there, which enters the runtime through haylen_main from the main.mm of an Xcode project or from AppleMain.cpp in CMake builds.
#define SOKOL_IMPL
#define SOKOL_NO_ENTRY
#include "sokol_app.h"
#include "sokol_gfx.h"
#include "sokol_glue.h"
#include "sokol_log.h"
