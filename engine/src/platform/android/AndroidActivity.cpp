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
std::atomic<float> AndroidActivity::statusBarHeight = 0.0F;
std::atomic<Orientation> AndroidActivity::orientation = Orientation::Landscape;

const GameActivity& AndroidActivity::getNative() {
    return *static_cast<const GameActivity*>(sapp_android_get_native_activity());
}

void AndroidActivity::setSafeAreaInsets(const math::Insets& value) {
    const std::scoped_lock lock(mutex);
    safeAreaInsets = value;
}

math::Insets AndroidActivity::toFramebuffer(const math::Insets& value) noexcept {
    const float scale = sapp_dpi_scale();
    return {.left = value.left * scale, .top = value.top * scale, .right = value.right * scale, .bottom = value.bottom * scale};
}

math::Rect AndroidActivity::toFramebuffer(const math::Rect& value) noexcept {
    const float scale = sapp_dpi_scale();
    return {value.x * scale, value.y * scale, value.width * scale, value.height * scale};
}

math::Insets AndroidActivity::getSafeAreaInsets() {
    const std::scoped_lock lock(mutex);
    return safeAreaInsets;
}

void AndroidActivity::setStatusBarHeight(float value) noexcept {
    statusBarHeight.store(value, std::memory_order_release);
}

float AndroidActivity::getStatusBarHeight() noexcept {
    return statusBarHeight.load(std::memory_order_acquire);
}

void AndroidActivity::receiveFold(const FoldReport& report) {
    std::optional<Fold> value;
    if (report.present) {
        const math::Rect bounds = math::Rect::fromMinMax({static_cast<float>(report.left), static_cast<float>(report.top)}, {static_cast<float>(report.right), static_cast<float>(report.bottom)});
        value = Fold{.bounds = bounds, .axis = report.vertical ? Fold::Axis::Vertical : Fold::Axis::Horizontal, .state = report.halfOpened ? Fold::State::HalfOpened : Fold::State::Flat, .separating = report.separating, .occluding = report.occluding};
    }
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
