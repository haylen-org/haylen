#include "haylen/port/PortDevice.hpp"

#include "haylen/port/GpuDevices.hpp"

namespace haylen::port {

// The device of the test port draws nothing and checks every call of the engine against the contract of the GPU device, running the code of the port that its shader tool wrote into the shader packages.
std::unique_ptr<gpu::Device> PortDevice::create(const std::optional<gpu::Surface>&, const gpu::DeviceOptions&) {
    return GpuDevices::createChecking(gpu::Device::Api::Port, gpu::Device::Language::Port);
}

} // namespace haylen::port
