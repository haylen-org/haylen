#include "gpu/DeviceFactory.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#if defined(HAYLEN_GPU_METAL)
#include "gpu/metal/Device.hpp"
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
#include "gpu/d3d12/Device.hpp"
#endif
#if defined(HAYLEN_GPU_DIRECT3D11)
#include "gpu/d3d11/Device.hpp"
#endif
#if defined(HAYLEN_GPU_VULKAN)
#include "gpu/vulkan/Device.hpp"
#endif
#if defined(HAYLEN_GPU_WEBGPU)
#include "gpu/webgpu/Device.hpp"
#endif
#if defined(HAYLEN_GPU_OPENGL) || defined(HAYLEN_GPU_OPENGLES) || defined(HAYLEN_GPU_WEBGL2)
#include "gpu/opengl/Device.hpp"
#endif

namespace haylen::gpu {

const std::vector<Device::Api> DeviceFactory::kCompiledApis = listCompiledApis();

std::vector<Device::Api> DeviceFactory::listCompiledApis() {
    std::vector<Device::Api> apis;
#if defined(HAYLEN_GPU_METAL)
    apis.push_back(Device::Api::Metal);
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
    apis.push_back(Device::Api::Direct3D12);
#endif
#if defined(HAYLEN_GPU_DIRECT3D11)
    apis.push_back(Device::Api::Direct3D11);
#endif
#if defined(HAYLEN_GPU_VULKAN)
    apis.push_back(Device::Api::Vulkan);
#endif
#if defined(HAYLEN_GPU_WEBGPU)
    apis.push_back(Device::Api::WebGpu);
#endif
#if defined(HAYLEN_GPU_OPENGL)
    apis.push_back(Device::Api::OpenGl);
#endif
#if defined(HAYLEN_GPU_OPENGLES)
    apis.push_back(Device::Api::OpenGlEs);
#endif
#if defined(HAYLEN_GPU_WEBGL2)
    apis.push_back(Device::Api::WebGl2);
#endif
    return apis;
}

std::span<const Device::Api> DeviceFactory::getCompiledApis() noexcept {
    return kCompiledApis;
}

std::vector<Device::Api> DeviceFactory::order(std::span<const Device::Api> preferred) {
    std::vector<Device::Api> ordered;
    ordered.reserve(kCompiledApis.size());
    // clang-format off
    const auto add = [&ordered](Device::Api api) {
        if (std::ranges::find(kCompiledApis, api) != kCompiledApis.end() && std::ranges::find(ordered, api) == ordered.end()) {
            ordered.push_back(api);
        }
    };
    // clang-format on
    std::ranges::for_each(preferred, add);
    std::ranges::for_each(kCompiledApis, add);
    return ordered;
}

bool DeviceFactory::usesGlContext(Device::Api api) noexcept {
    return api == Device::Api::OpenGl || api == Device::Api::OpenGlEs || api == Device::Api::WebGl2;
}

std::string_view DeviceFactory::getApiName(Device::Api api) noexcept {
    const auto found = std::ranges::find(Device::kApiNames, api, &std::pair<std::string_view, Device::Api>::second);
    return found == Device::kApiNames.end() ? Device::kApiNames.front().first : found->first;
}

std::optional<Device::Api> DeviceFactory::findApi(std::string_view name) noexcept {
    const auto found = std::ranges::find(Device::kApiNames, name, &std::pair<std::string_view, Device::Api>::first);
    if (found == Device::kApiNames.end() || found->second == Device::Api::None) {
        return std::nullopt;
    }
    return found->second;
}

// The surface and the options reach only the devices that the build compiled.
std::unique_ptr<Device> DeviceFactory::create(Device::Api api, [[maybe_unused]] const std::optional<Surface>& surface, [[maybe_unused]] const Options& options) {
    switch (api) {
#if defined(HAYLEN_GPU_METAL)
    case Device::Api::Metal:
        return metal::Device::create(surface, options);
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
    case Device::Api::Direct3D12:
        return d3d12::Device::create(surface, options);
#endif
#if defined(HAYLEN_GPU_DIRECT3D11)
    case Device::Api::Direct3D11:
        return d3d11::Device::create(surface, options);
#endif
#if defined(HAYLEN_GPU_VULKAN)
    case Device::Api::Vulkan:
        return vulkan::Device::create(surface, options);
#endif
#if defined(HAYLEN_GPU_WEBGPU)
    case Device::Api::WebGpu:
        return webgpu::Device::create(surface, options);
#endif
#if defined(HAYLEN_GPU_OPENGL)
    case Device::Api::OpenGl:
        return opengl::Device::create(api, surface, options);
#endif
#if defined(HAYLEN_GPU_OPENGLES)
    case Device::Api::OpenGlEs:
        return opengl::Device::create(api, surface, options);
#endif
#if defined(HAYLEN_GPU_WEBGL2)
    case Device::Api::WebGl2:
        return opengl::Device::create(api, surface, options);
#endif
    default:
        break;
    }
    throw std::runtime_error(std::format("This build has no device of the graphics API \"{}\". Add it to \"HAYLEN_GRAPHICS_APIS\" when the engine is built.", getApiName(api)));
}

} // namespace haylen::gpu
