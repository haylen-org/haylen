#include "platform/GraphicsApiFallback.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "gpu/DeviceFactory.hpp"
#include "haylen/core/Log.hpp"

namespace haylen::platform {

GraphicsApiFallback::GraphicsApiFallback(std::vector<gpu::Device::Api> order, std::string reason) : apis(std::move(order)), failure(std::move(reason)) {}

std::unique_ptr<gpu::Device> GraphicsApiFallback::open(Window& window) {
    for (; next < apis.size(); ++next) {
        const gpu::Device::Api api = apis[next];
        if (!window.prepare(api)) {
            return nullptr;
        }

        try {
            if (window.getApi() != api) {
                window.open(api);
            }

            if (!window.hasArea()) {
                return nullptr;
            }

            std::unique_ptr<gpu::Device> device = window.createDevice(api);
            core::Log::info("The app draws through \"{}\".", gpu::DeviceFactory::getApiName(api));
            return device;
        } catch (const std::exception& error) {
            core::Log::warning("The graphics API \"{}\" could not start. {}", gpu::DeviceFactory::getApiName(api), error.what());
            window.close();
        }
    }

    throw std::runtime_error(failure);
}

} // namespace haylen::platform
