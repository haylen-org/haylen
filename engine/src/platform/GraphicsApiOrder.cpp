#include "platform/GraphicsApiOrder.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <stdexcept>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::platform {

// The APIs of `app.json` may belong to other platforms, which the order leaves out quietly, while an API the command line asks for and the build lacks deserves a word.
std::vector<gpu::Device::Api> GraphicsApiOrder::resolve(std::string_view requested, std::span<const gpu::Device::Api> configured) {
    if (requested.empty()) {
        return gpu::DeviceFactory::order(configured);
    }

    const std::optional<gpu::Device::Api> api = gpu::DeviceFactory::findApi(requested);
    if (!api) {
        throw std::invalid_argument(std::format("The graphics API \"{}\" of \"--graphics-api\" is unknown. It is \"metal\", \"direct3D12\", \"direct3D11\", \"vulkan\", \"openGl\", \"openGlEs\", \"webGpu\", \"webGl2\" or \"port\".", requested));
    }
    const std::span<const gpu::Device::Api> compiled = gpu::DeviceFactory::getCompiledApis();
    if (std::ranges::find(compiled, *api) == compiled.end()) {
        core::Log::warning("This build has no device of the graphics API \"{}\" that \"--graphics-api\" asks for, so the app draws through another one.", requested);
    }
    return gpu::DeviceFactory::order(std::span(&*api, 1));
}

} // namespace haylen::platform
