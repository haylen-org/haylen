#pragma once

#include <optional>
#include <utility>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::platform {

// The graphics APIs that the GPU devices of an Android activity try in turn. An API that failed to start is not tried again while the activity lives. The device that replaces one that stopped working, after a reset of the GPU or a lost context, tries the same order again, and a device of the lost API then starts on a new context, since a lost context takes no more commands. It compiles on every platform, so the tests of every host check it.
class AndroidGraphicsApis final {
  public:
    void setOrder(std::vector<gpu::Device::Api> value) {
        order = std::move(value);
    }

    [[nodiscard]] const std::vector<gpu::Device::Api>& getOrder() const noexcept {
        return order;
    }

    [[nodiscard]] bool hasFailed(gpu::Device::Api api) const noexcept;
    void fail(gpu::Device::Api api);

    // Records that the device of the API stopped working.
    void lose(gpu::Device::Api api) noexcept {
        lost = api;
    }

    // Whether the next device of the API starts on a new context, which it answers once after a loss.
    [[nodiscard]] bool takeRenewal(gpu::Device::Api api) noexcept;

  private:
    std::vector<gpu::Device::Api> order;
    std::vector<gpu::Device::Api> failed;
    std::optional<gpu::Device::Api> lost;
};

} // namespace haylen::platform
