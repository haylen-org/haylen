#pragma once

#include <mutex>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::graphics {

// Collects the GPU objects whose owners died, from any thread, so they are destroyed on the frame thread once no queued draw can use them.
class ResourceGraveyard final {
  public:
    void bury(gpu::Device::Image image);
    void bury(gpu::Device::Buffer buffer);
    void bury(gpu::Device::Program program);
    void bury(gpu::Device::Pipeline pipeline);
    void collect(gpu::Device& device);

  private:
    std::mutex mutex;
    std::vector<gpu::Device::Image> images;
    std::vector<gpu::Device::Buffer> buffers;
    std::vector<gpu::Device::Program> programs;
    std::vector<gpu::Device::Pipeline> pipelines;
};

} // namespace haylen::graphics
