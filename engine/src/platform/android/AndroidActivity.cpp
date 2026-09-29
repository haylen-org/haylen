#include "platform/android/AndroidActivity.hpp"

#include "sokol_app.h"

namespace haylen::platform {

std::mutex AndroidActivity::mutex;
math::Insets AndroidActivity::safeAreaInsets{};
std::atomic<bool> AndroidActivity::framePresented = false;
std::atomic<bool> AndroidActivity::television = false;
std::atomic<Orientation> AndroidActivity::orientation = Orientation::Landscape;

const ANativeActivity& AndroidActivity::getNative() {
    return *static_cast<const ANativeActivity*>(sapp_android_get_native_activity());
}

void AndroidActivity::setSafeAreaInsets(const math::Insets& value) {
    const std::scoped_lock lock(mutex);
    safeAreaInsets = value;
}

math::Insets AndroidActivity::getSafeAreaInsets() {
    const std::scoped_lock lock(mutex);
    return safeAreaInsets;
}

void AndroidActivity::setOrientation(Orientation value) noexcept {
    orientation.store(value, std::memory_order_release);
}

Orientation AndroidActivity::getOrientation() noexcept {
    return orientation.load(std::memory_order_acquire);
}

void AndroidActivity::setTelevision(bool value) noexcept {
    television.store(value, std::memory_order_release);
}

bool AndroidActivity::isTelevision() noexcept {
    return television.load(std::memory_order_acquire);
}

void AndroidActivity::endSplashScreen() noexcept {
    framePresented.store(true, std::memory_order_release);
}

bool AndroidActivity::isFramePresented() noexcept {
    return framePresented.load(std::memory_order_acquire);
}

} // namespace haylen::platform
