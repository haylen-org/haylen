#include "platform/android/AndroidActivity.hpp"

#include "sokol_app.h"

namespace haylen::platform {

std::mutex& AndroidActivity::mutex = *new std::mutex();
math::Insets AndroidActivity::safeAreaInsets{};
std::optional<Fold> AndroidActivity::fold;
std::atomic<bool> AndroidActivity::framePresented = false;
std::atomic<float> AndroidActivity::splashFadeOut = 0.0F;
std::atomic<bool> AndroidActivity::television = false;
std::atomic<float> AndroidActivity::density = 1.0F;
std::atomic<Orientation> AndroidActivity::orientation = Orientation::Landscape;

const GameActivity& AndroidActivity::getNative() {
    return *static_cast<const GameActivity*>(sapp_android_get_native_activity());
}

void AndroidActivity::setSafeAreaInsets(const math::Insets& value) {
    const std::scoped_lock lock(mutex);
    safeAreaInsets = value;
}

math::Insets AndroidActivity::getSafeAreaInsets() {
    const std::scoped_lock lock(mutex);
    return safeAreaInsets;
}

void AndroidActivity::setFold(std::optional<Fold> value) {
    const std::scoped_lock lock(mutex);
    fold = value;
}

std::optional<Fold> AndroidActivity::getFold() {
    const std::scoped_lock lock(mutex);
    return fold;
}

void AndroidActivity::setDensity(float value) noexcept {
    density.store(value, std::memory_order_release);
}

float AndroidActivity::getDensity() noexcept {
    return density.load(std::memory_order_acquire);
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

void AndroidActivity::endSplashScreen(float fadeOutSeconds) noexcept {
    splashFadeOut.store(fadeOutSeconds, std::memory_order_relaxed);
    framePresented.store(true, std::memory_order_release);
}

float AndroidActivity::getSplashFadeOut() noexcept {
    return splashFadeOut.load(std::memory_order_relaxed);
}

void AndroidActivity::holdSplashScreen() noexcept {
    framePresented.store(false, std::memory_order_release);
}

bool AndroidActivity::isFramePresented() noexcept {
    return framePresented.load(std::memory_order_acquire);
}

} // namespace haylen::platform
