#include "port/PortHost.hpp"

#include <utility>

namespace haylen::port {

math::Vec2 PortHost::getFramebufferSize() const noexcept {
    return host->getFramebufferSize();
}

float PortHost::getDpiScale() const noexcept {
    return host->getDpiScale();
}

bool PortHost::isFullscreen() const noexcept {
    return host->isFullscreen();
}

void PortHost::setFullscreen(bool value, std::function<void(platform::Permission::Answer answer)> callback) {
    host->setFullscreen(value, std::move(callback));
}

void PortHost::setTitle(std::string_view value) {
    host->setTitle(value);
}

void PortHost::setCursor(Cursor value) {
    host->setCursor(value);
}

void PortHost::setCursorVisible(bool value) {
    host->setCursorVisible(value);
}

void PortHost::setMouseLocked(bool value, std::function<void(platform::Permission::Answer answer)> callback) {
    host->setMouseLocked(value, std::move(callback));
}

void PortHost::setClipboard(std::string_view value, std::function<void(platform::Permission::Answer answer)> callback) {
    host->setClipboard(value, std::move(callback));
}

std::string PortHost::getClipboard() const {
    return host->getClipboard();
}

void PortHost::readClipboard(std::function<void(platform::Permission::Answer answer, std::string text)> callback) {
    host->readClipboard(std::move(callback));
}

bool PortHost::canBeTransparent() const noexcept {
    return host->canBeTransparent();
}

std::unique_ptr<gpu::Device> PortHost::createGpuDevice() {
    return host->createGpuDevice();
}

bool PortHost::prepareGpuDevice() {
    return host->prepareGpuDevice();
}

audio::Mixer::Setup PortHost::getAudioSetup() const {
    return host->getAudioSetup();
}

void PortHost::closeWindow() {
    host->close();
}

void PortHost::endSplashScreen(float fadeOutSeconds) {
    host->endSplashScreen(fadeOutSeconds);
}

bool PortHost::pumpHaltedEvents() {
    return host->pumpHaltedEvents();
}

void PortHost::pollGamepads(std::span<input::GamepadState> gamepads) {
    host->pollGamepads(gamepads);
}

void PortHost::rumbleGamepad(std::size_t index, float low, float high, float seconds) {
    host->rumbleGamepad(index, low, high, seconds);
}

void PortHost::setGamepadMotion(input::GamepadMotion::Sensors sensors) {
    host->setGamepadMotion(sensors);
}

bool PortHost::allowsQuit() const noexcept {
    return host->canQuit();
}

void PortHost::requestWindowClose() {
    host->requestClose();
}

void PortHost::setWindowFocusable(bool value) {
    host->setFocusable(value);
}

void PortHost::setScreenSaverSuspended(bool value, std::function<void(platform::Permission::Answer answer)> callback) {
    host->setScreenSaverSuspended(value, std::move(callback));
}

void PortHost::applyFrameRate(int framesPerSecond) {
    host->applyFrameRate(framesPerSecond);
}

void PortHost::applyMaxPixelDensity(float cap) {
    host->applyMaxPixelDensity(cap);
}

} // namespace haylen::port
