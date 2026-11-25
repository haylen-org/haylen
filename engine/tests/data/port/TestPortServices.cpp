#include "TestPortServices.hpp"

#include <stdexcept>

#include "TestPortWindow.hpp"
#include "haylen/io/Package.hpp"

namespace haylen::platform {

port::Services TestPortServices::create(std::filesystem::path executableFolder, std::shared_ptr<port::AudioOutput> audio) {
    const auto services = std::make_shared<TestPortServices>(std::move(executableFolder));
    return {.platform = services, .displays = services, .audio = std::move(audio)};
}

std::string_view TestPortServices::getName() const noexcept {
    return "test-port";
}

// The test port follows no theme, battery or languages of a system, so every app hears the defaults of the engine.
void TestPortServices::initialize() {}

void TestPortServices::shutdown() noexcept {}

// The player of the port plays the package that its command line names, and an app that `haylen_add_app` built plays the one next to it.
std::shared_ptr<io::Package> TestPortServices::openBundledPackage() {
    const std::filesystem::path folder = executableFolder / "app";
    if (!std::filesystem::is_directory(folder)) {
        throw std::runtime_error("The test port finds no app in \"" + folder.string() + "\". Name the package of an app on the command line, or build the app with \"haylen_add_app\".");
    }

    return io::Package::openDirectory(folder);
}

std::filesystem::path TestPortServices::getUserDataDirectory(std::string_view identifier) {
    return std::filesystem::temp_directory_path() / "haylen-test-port" / std::string(identifier) / "data";
}

// Every write of the test port lands in a folder of the machine that builds it, which saves it.
void TestPortServices::persistUserData() {}

// Nothing keeps the user data of the apps from them on this platform.
std::string TestPortServices::getUserDataRefusal() {
    return {};
}

SystemInfo TestPortServices::getSystemInfo() {
    return {.os = SystemInfo::Os::Port, .osVersion = "1.0", .deviceModel = "Test port", .deviceKind = SystemInfo::DeviceKind::Tv};
}

// The test port has a screen and a controller, like a console on a TV, rather than a mouse or a touch screen.
bool TestPortServices::hasPointerDevice() const noexcept {
    return false;
}

// The test port takes no keyboard.
bool TestPortServices::hasKeyRepeats() const noexcept {
    return false;
}

std::vector<Display> TestPortServices::getDisplays() {
    return {Display{.id = "test-port", .name = "Test port", .bounds = getWindowFrame(), .workArea = getWindowFrame(), .scale = 1.0F, .primary = true}};
}

std::string TestPortServices::getWindowDisplayId() {
    return "test-port";
}

bool TestPortServices::hasUnlistedDisplays() {
    return false;
}

void TestPortServices::requestEveryScreen(std::function<void(platform::SystemAnswer answer)> callback) {
    callback({.state = platform::SystemAnswer::State::Granted});
}

math::Rect TestPortServices::getWindowFrame() {
    return {0.0F, 0.0F, TestPortWindow::kSize.x, TestPortWindow::kSize.y};
}

} // namespace haylen::platform
