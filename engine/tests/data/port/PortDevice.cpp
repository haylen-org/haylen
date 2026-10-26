#include "gpu/PortDevice.hpp"

#include "gpu/NullDevice.hpp"

namespace haylen::gpu {

// The device of the test port draws nothing and checks every call of the engine against the contract of the GPU device, running the code of the port that its shader tool wrote into the shader packages.
std::unique_ptr<Device> PortDevice::create(const std::optional<Surface>&, const DeviceFactory::Options&) {
    return std::make_unique<NullDevice>(Device::Language::Port, NullDevice::getDefaultCapabilities(), Device::Api::Port);
}

} // namespace haylen::gpu
