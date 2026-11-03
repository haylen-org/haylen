#include "platform/DeviceRecovery.hpp"

#include <format>
#include <string_view>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::platform {

std::optional<std::string> DeviceRecovery::recordLoss(gpu::Device::Api api) {
    const std::string_view name = gpu::DeviceFactory::getApiName(api);
    if (replaced) {
        return std::format("The GPU device of \"{}\" stopped working again after it replaced a lost one, so the app cannot draw.", name);
    }
    replaced = true;
    core::Log::warning("The GPU device of \"{}\" stopped working, so the app starts again on a new one.", name);
    return std::nullopt;
}

} // namespace haylen::platform
