#include "haylen/port/GpuDevices.hpp"

#include "gpu/DeviceFactory.hpp"
#include "gpu/NullDevice.hpp"

namespace haylen::port {

bool GpuDevices::prepare(gpu::Device::Api api, const gpu::DeviceOptions& options) {
    return gpu::DeviceFactory::prepare(api, options);
}

std::unique_ptr<gpu::Device> GpuDevices::create(gpu::Device::Api api, const std::optional<gpu::Surface>& surface, const gpu::DeviceOptions& options) {
    return gpu::DeviceFactory::create(api, surface, options);
}

std::unique_ptr<gpu::Device> GpuDevices::createChecking(gpu::Device::Api api, gpu::Device::Language language) {
    return std::make_unique<gpu::NullDevice>(language, gpu::NullDevice::getDefaultCapabilities(), api);
}

std::string_view GpuDevices::getApiName(gpu::Device::Api api) noexcept {
    return gpu::DeviceFactory::getApiName(api);
}

} // namespace haylen::port
