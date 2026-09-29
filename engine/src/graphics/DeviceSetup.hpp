#pragma once

#include "sokol_gfx.h"

namespace haylen::graphics {

// What the platform host hands the device to set up the GPU context.
struct DeviceSetup {
    sg_environment environment{};
    sg_logger logger{};
    bool validation = true;
};

} // namespace haylen::graphics
