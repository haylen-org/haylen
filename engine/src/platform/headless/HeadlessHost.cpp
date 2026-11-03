#include "platform/headless/HeadlessHost.hpp"

#include <algorithm>
#include <stdexcept>
#include <thread>
#include <utility>

#include "platform/FullscreenTarget.hpp"

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

// The window moves to the display it covers, as a desktop window does.
void HeadlessHost::enterFullscreen(const Fullscreen& value) {
    const FullscreenTarget target = FullscreenTarget::resolve(displays, getDisplayId(), value);
    frame = target.display.bounds;
    fullscreen = true;
    fullscreenTarget = {.display = target.display.id, .mode = target.mode};
}

std::unique_ptr<gpu::Device> HeadlessHost::createGpuDevice() {
    if (gpuDeviceMaker) {
        gpu = nullptr;
        return gpuDeviceMaker();
    }
    auto device = std::make_unique<gpu::NullDevice>();
    gpu = device.get();
    return device;
}

void HeadlessHost::setTransparent(bool value) {
    if (value && !transparencySupported) {
        throw std::logic_error("The window opened opaque, so it cannot turn transparent. Set \"window.transparent\" in \"app.json\" to open a window that can.");
    }
    transparent = value;
}

bool HeadlessHost::isFrameTransparent() const noexcept {
    return transparent;
}

std::filesystem::path HeadlessHost::getUserDataDirectory(std::string_view identifier) {
    return dataDirectory / std::string(identifier);
}

void HeadlessHost::pollGamepads(std::span<input::GamepadState> states) {
    std::copy_n(gamepads.begin(), std::min(states.size(), gamepads.size()), states.begin());
}

void HeadlessHost::requestPermission(Permission::Kind kind, std::function<void(Permission::Answer answer)> callback) {
    permissionRequests.push_back(kind);
    if (permissionAnswersHeld) {
        heldPermissionAnswers.push_back(std::move(callback));
        return;
    }
    callback(permissionAnswer);
}

void HeadlessHost::answerPermissions() {
    for (const std::function<void(Permission::Answer)>& callback : std::exchange(heldPermissionAnswers, {})) {
        callback(permissionAnswer);
    }
}

void HeadlessHost::dispatchPlatformCall(std::uint64_t id, std::string_view method, std::string_view paramsJson, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) {
    platformCalls.push_back({.id = id, .method = std::string(method), .paramsJson = std::string(paramsJson), .buffers = {buffers.begin(), buffers.end()}, .views = {views.begin(), views.end()}});
}

void HeadlessHost::openUrl(std::string_view url, std::function<void(Permission::Answer answer)> callback) {
    openedUrls.emplace_back(url);
    callback(urlAnswer);
}

} // namespace haylen::platform
