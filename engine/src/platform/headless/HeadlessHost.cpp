#include "platform/headless/HeadlessHost.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>

namespace haylen::platform {

HeadlessHost::HeadlessHost(std::filesystem::path directory, math::Vec2 size) : dataDirectory(std::move(directory)), framebufferSize(size), frame{0.0F, 0.0F, size.x, size.y} {
#if defined(__APPLE__)
    systemInfo.os = SystemInfo::Os::MacOs;
#elif defined(_WIN32)
    systemInfo.os = SystemInfo::Os::Windows;
#else
    systemInfo.os = SystemInfo::Os::Linux;
#endif
    systemInfo.deviceKind = SystemInfo::DeviceKind::Desktop;
    systemInfo.cpuCores = static_cast<int>(std::thread::hardware_concurrency());
}

graphics::DeviceSetup HeadlessHost::getGraphicsSetup() {
    graphics::DeviceSetup setup;
    setup.environment.defaults.color_format = SG_PIXELFORMAT_RGBA8;
    setup.environment.defaults.depth_format = SG_PIXELFORMAT_NONE;
    setup.environment.defaults.sample_count = 1;
    return setup;
}

void HeadlessHost::setTransparent(bool value) {
    if (value && !transparencySupported) {
        throw std::logic_error("The window opened opaque, so it cannot turn transparent. Set window.transparent in app.json to open a window that can.");
    }
    transparent = value;
}

graphics::FrameTarget HeadlessHost::getFrameTarget() {
    graphics::FrameTarget target;
    target.transparent = transparent;
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

void HeadlessHost::openUrl(std::string_view url, std::function<void(bool opened)> callback) {
    openedUrls.emplace_back(url);
    callback(opensUrls);
}

} // namespace haylen::platform
