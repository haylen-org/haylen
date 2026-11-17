#include "haylen/port/Player.hpp"

#include <utility>

#include "platform/Player.hpp"
#include "platform/Services.hpp"
#include "port/PortHost.hpp"

namespace haylen::port {

void Player::installServices(Services services) {
    platform::Services::install(std::move(services));
}

void Player::create(int argc, char* argv[], std::unique_ptr<Host> host) {
    platform::Player::create(argc, argv, std::make_unique<PortHost>(std::move(host)));
}

const std::vector<gpu::Device::Api>& Player::getGraphicsApis() noexcept {
    return platform::Player::getGraphicsApis();
}

void Player::initialize() {
    platform::Player::initialize();
}

void Player::frame(double seconds) {
    platform::Player::frame(seconds);
}

void Player::cleanup() {
    platform::Player::cleanup();
}

void Player::handleEvent(const platform::Event& event) {
    platform::Player::handleEvent(event);
}

void Player::postEvent(platform::Event event) {
    platform::Player::postEvent(std::move(event));
}

gpu::Device* Player::getGpuDevice() noexcept {
    return platform::Player::getGpuDevice();
}

int Player::getExitStatus() noexcept {
    return platform::Player::getExitStatus();
}

} // namespace haylen::port
