#pragma once

#include <span>
#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::platform {

// The order in which a player tries the graphics APIs of the build when it opens its window: the API of the command line, or else the ones of `window.graphicsApis` of `app.json`, then the other APIs of the build in the order of the platform.
class GraphicsApiOrder final {
  public:
    // Takes the name that `--graphics-api` gives, empty when the command line names none, and the APIs of `app.json`. Throws `std::invalid_argument` for a name that is not a graphics API. The order leaves out the APIs the build did not compile, and logs a warning when the command line asks for one.
    [[nodiscard]] static std::vector<gpu::Device::Api> resolve(std::string_view requested, std::span<const gpu::Device::Api> configured);
};

} // namespace haylen::platform
