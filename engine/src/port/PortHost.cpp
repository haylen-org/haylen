#include "port/PortHost.hpp"

#include <memory>
#include <utility>

#include "platform/Services.hpp"

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

void PortHost::applyFullscreen(bool value, std::function<void(platform::SystemAnswer answer)> callback) {
    host->setFullscreen(value, std::move(callback));
}

void PortHost::setTitle(std::string_view value) {
    host->setTitle(value);
}

void PortHost::setCursor(Cursor value) {
    host->setCursor(value);
}

void PortHost::applyMouseMode(MouseMode value, std::function<void(platform::SystemAnswer answer)> callback) {
    host->setMouseMode(value, std::move(callback));
}

void PortHost::setClipboard(std::string_view value, std::function<void(platform::SystemAnswer answer)> callback) {
    host->setClipboard(value, std::move(callback));
}

std::string PortHost::getClipboard() const {
    return host->getClipboard();
}

void PortHost::readClipboard(std::function<void(platform::SystemAnswer answer, std::string text)> callback) {
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

// A port plays through the audio output it registered, and the mixer of a port without one keeps time without sound.
audio::Mixer::Setup PortHost::getAudioSetup() const {
    std::shared_ptr<AudioOutput> output = platform::Services::getAudioOutput();
    const bool device = output != nullptr;
    return {.device = device, .keepsTime = !device, .output = std::move(output)};
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

void PortHost::setScreenSaverSuspended(bool value, std::function<void(platform::SystemAnswer answer)> callback) {
    host->setScreenSaverSuspended(value, std::move(callback));
}

void PortHost::applyFrameRate(int framesPerSecond) {
    host->applyFrameRate(framesPerSecond);
}

void PortHost::applyMaxPixelDensity(float cap) {
    host->applyMaxPixelDensity(cap);
}

} // namespace haylen::port
