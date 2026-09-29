#include "platform/headless/HeadlessHost.hpp"

#include <algorithm>

namespace haylen::platform {

HeadlessHost::HeadlessHost(std::filesystem::path directory, math::Vec2 size) : dataDirectory(std::move(directory)), framebufferSize(size) {}

graphics::DeviceSetup HeadlessHost::getGraphicsSetup() {
    graphics::DeviceSetup setup;
    setup.environment.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    setup.environment.defaults.depth_format = SG_PIXELFORMAT_NONE;
    setup.environment.defaults.sample_count = 1;
    return setup;
}

graphics::FrameTarget HeadlessHost::getFrameTarget() {
    graphics::FrameTarget target;
    target.swapchain.width = static_cast<int>(framebufferSize.x);
    target.swapchain.height = static_cast<int>(framebufferSize.y);
    target.swapchain.sample_count = 1;
    target.swapchain.color_format = SG_PIXELFORMAT_RGBA8;
    target.swapchain.depth_format = SG_PIXELFORMAT_NONE;
    return target;
}

std::filesystem::path HeadlessHost::getUserDataDirectory(std::string_view identifier) {
    return dataDirectory / std::string(identifier);
}

void HeadlessHost::pollGamepads(std::span<input::GamepadState> states) {
    std::copy_n(gamepads.begin(), std::min(states.size(), gamepads.size()), states.begin());
}

void HeadlessHost::dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson) {
    platformCalls.push_back({id, std::string(method), std::string(paramsJson)});
}

} // namespace haylen::platform
