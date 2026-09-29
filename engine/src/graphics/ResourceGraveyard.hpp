#pragma once

#include <mutex>
#include <vector>

#include "sokol_gfx.h"

namespace haylen::graphics {

// Collects GPU objects whose owners died so they are destroyed on the frame thread once no queued draw can use them.
class ResourceGraveyard final {
  public:
    void bury(sg_image image, sg_view view);
    void buryView(sg_view view);
    void buryBuffer(sg_buffer buffer);
    void buryShader(sg_shader shader);
    void buryPipeline(sg_pipeline pipeline);
    void collect();

  private:
    std::mutex mutex;
    std::vector<sg_image> images;
    std::vector<sg_view> views;
    std::vector<sg_buffer> buffers;
    std::vector<sg_shader> shaders;
    std::vector<sg_pipeline> pipelines;
};

} // namespace haylen::graphics
