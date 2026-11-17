#pragma once

namespace haylen::gpu {

// How a GPU device starts, which every device reads when it is created.
struct DeviceOptions {
    // Whether the device turns on the validation of its graphics API and the labels of its objects, which Debug builds and tests do.
    bool validation = false;

    // Whether the device prefers the software rasterizer of its graphics API, such as WARP on Direct3D, which draws where no GPU does and the same pixels on every machine. A device whose API has no software rasterizer ignores it.
    bool software = false;

    // Whether the device prefers the GPU that uses the least power, such as the integrated GPU of a laptop that has two, to the most powerful one. A device whose API does not tell its GPUs apart ignores it.
    bool lowPower = false;
};

} // namespace haylen::gpu
