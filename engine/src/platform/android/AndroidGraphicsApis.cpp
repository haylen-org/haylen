#include "platform/android/AndroidGraphicsApis.hpp"

#include <algorithm>

namespace haylen::platform {

bool AndroidGraphicsApis::hasFailed(gpu::Device::Api api) const noexcept {
    return std::ranges::find(failed, api) != failed.end();
}

void AndroidGraphicsApis::fail(gpu::Device::Api api) {
    if (!hasFailed(api)) {
        failed.push_back(api);
    }
}

bool AndroidGraphicsApis::takeRenewal(gpu::Device::Api api) noexcept {
    if (lost != api) {
        return false;
    }
    lost.reset();
    return true;
}

} // namespace haylen::platform
