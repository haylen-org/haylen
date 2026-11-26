#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/port/Host.hpp"
#include "haylen/port/Services.hpp"
#include "platform/Player.hpp"
#include "platform/Services.hpp"
#include "port/PortHost.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"

// The player of these tests makes the app of a test application, as an app makes its own.
std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<haylen::test::TestApplication>(nullptr);
}

namespace haylen::platform {

// The player starts in a process of its own for each test, since the services of the platform and the player belong to the process.
class PlayerTest : public ::testing::Test {
  protected:
    // The ways a run of the player in its own process ends, which the process returns as its status.
    enum class Outcome : int {
        Clean = 0,
        Created = 1,
        HostKept = 2,
        ServicesKept = 3,
        DisplayNotWaited = 4,
    };

    // The frames that the player plays in the test of its waits for the display.
    static constexpr int kPlayedFrames = 5;

    // A window of a port that has no pixels, input or sound, which tells the test when the player let go of it.
    class Window final : public port::Host {
      public:
        explicit Window(bool& gone) noexcept : destroyed(gone) {}

        // A window whose GPU device the player takes when it starts the app.
        Window(bool& gone, std::unique_ptr<gpu::Device> made) noexcept : destroyed(gone), device(std::move(made)) {}

        ~Window() override {
            destroyed = true;
        }

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        [[nodiscard]] math::Vec2 getFramebufferSize() const noexcept override {
            return {64.0F, 64.0F};
        }

        [[nodiscard]] float getDpiScale() const noexcept override {
            return 1.0F;
        }

        [[nodiscard]] bool isFullscreen() const noexcept override {
            return false;
        }

        void setFullscreen(bool, std::function<void(SystemAnswer answer)> callback) override {
            callback({.state = SystemAnswer::State::Unsupported});
        }

        void setTitle(std::string_view) override {}

        void setCursor(platform::Window::Cursor) override {}

        void setMouseMode(platform::Window::MouseMode, std::function<void(SystemAnswer answer)> callback) override {
            callback({.state = SystemAnswer::State::Unsupported});
        }

        void setClipboard(std::string_view, std::function<void(SystemAnswer answer)> callback) override {
            callback({.state = SystemAnswer::State::Unsupported});
        }

        [[nodiscard]] std::string getClipboard() const override {
            return {};
        }

        void readClipboard(std::function<void(SystemAnswer answer, std::string text)> callback) override {
            callback({.state = SystemAnswer::State::Unsupported}, {});
        }

        [[nodiscard]] bool canBeTransparent() const noexcept override {
            return false;
        }

        [[nodiscard]] TextInput& getTextInput() noexcept override {
            return textInput;
        }

        [[nodiscard]] bool prepareGpuDevice() override {
            return true;
        }

        [[nodiscard]] std::unique_ptr<gpu::Device> createGpuDevice() override {
            return std::move(device);
        }

        void requestClose() override {}

        void close() override {}

        [[nodiscard]] bool canQuit() const noexcept override {
            return true;
        }

        void endSplashScreen(float) override {}

        [[nodiscard]] bool pumpHaltedEvents() override {
            return false;
        }

        void pollGamepads(std::span<input::GamepadState>) override {}

        void rumbleGamepad(std::size_t, float, float, float) override {}

        void setGamepadMotion(input::GamepadMotion::Sensors) override {}

        void setFocusable(bool) override {}

        void setScreenSaverSuspended(bool, std::function<void(SystemAnswer answer)> callback) override {
            callback({.state = SystemAnswer::State::Unsupported});
        }

        void applyFrameRate(int) override {}

        void applyMaxPixelDensity(float) override {}

      private:
        bool& destroyed;
        std::unique_ptr<gpu::Device> device;
        TextInput textInput;
    };

    // A platform with one screen, which counts how often the player starts and ends it, and whose links cannot be listened for where it refuses them, as when the system refuses the channel that the links of later processes come through.
    class Platform final : public port::PlatformService, public port::DisplayService, public port::LinkService {
      public:
        explicit Platform(bool refusing) noexcept : refusesLinks(refusing) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "player-test";
        }

        void initialize() override {
            ++initialized;
        }

        void shutdown() noexcept override {
            ++shutDown;
        }

        [[nodiscard]] std::shared_ptr<io::Package> openBundledPackage() override {
            throw std::runtime_error("The platform of the player tests ships no package.");
        }

        [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override {
            return std::filesystem::temp_directory_path() / "haylen-player-tests" / std::string(identifier);
        }

        void persistUserData() override {}

        [[nodiscard]] std::string getUserDataRefusal() override {
            return {};
        }

        [[nodiscard]] SystemInfo getSystemInfo() override {
            return {.os = SystemInfo::Os::Port};
        }

        [[nodiscard]] bool hasPointerDevice() const noexcept override {
            return false;
        }

        [[nodiscard]] bool hasKeyRepeats() const noexcept override {
            return false;
        }

        [[nodiscard]] std::vector<Display> getDisplays() override {
            return {Display{.id = "screen", .bounds = getWindowFrame(), .workArea = getWindowFrame(), .scale = 1.0F, .primary = true}};
        }

        [[nodiscard]] std::string getWindowDisplayId() override {
            return "screen";
        }

        [[nodiscard]] bool hasUnlistedDisplays() override {
            return false;
        }

        void requestEveryScreen(std::function<void(SystemAnswer answer)> callback) override {
            callback({.state = SystemAnswer::State::Granted});
        }

        [[nodiscard]] math::Rect getWindowFrame() override {
            return {0.0F, 0.0F, 64.0F, 64.0F};
        }

        [[nodiscard]] bool forwardLink(std::string_view, std::string_view) override {
            return false;
        }

        void listenForLinks(std::string_view) override {
            if (refusesLinks) {
                throw std::runtime_error("The platform of the player tests refuses to listen for links.");
            }
        }

        int initialized = 0;
        int shutDown = 0;

      private:
        bool refusesLinks = false;
    };

    // Creates the player on a platform whose last step of the creation fails, and tells what the failure left behind.
    [[nodiscard]] static Outcome createOnFailingPlatform() {
        const auto platform = std::make_shared<Platform>(true);
        Services::install({.platform = platform, .displays = platform, .links = platform});

        bool windowGone = false;
        std::string program = "haylen";
        std::array<char*, 1> arguments{program.data()};
        bool refused = false;
        try {
            Player::create(static_cast<int>(arguments.size()), arguments.data(), std::make_unique<port::PortHost>(std::make_unique<Window>(windowGone)));
        } catch (const std::runtime_error&) {
            refused = true;
        }

        if (!refused) {
            return Outcome::Created;
        }

        if (!windowGone) {
            return Outcome::HostKept;
        }

        return platform->initialized == 1 && platform->shutDown == 1 ? Outcome::Clean : Outcome::ServicesKept;
    }

    // Plays frames of an app that draws every frame, and tells whether the player waited for the display after each one.
    [[nodiscard]] static Outcome playFrames() {
        const auto platform = std::make_shared<Platform>(false);
        Services::install({.platform = platform, .displays = platform, .links = platform});

        const test::TemporaryDirectory folder;
        folder.write("haylen.json", R"({"name": "Display Waits"})");
        auto made = std::make_unique<gpu::NullDevice>();
        const gpu::NullDevice& device = *made;
        bool windowGone = false;
        std::string program = "haylen";
        std::string package = folder.getPath().string();
        std::array<char*, 2> arguments{program.data(), package.data()};
        Player::create(static_cast<int>(arguments.size()), arguments.data(), std::make_unique<port::PortHost>(std::make_unique<Window>(windowGone, std::move(made))));
        Player::initialize();

        for (int frame = 0; frame < kPlayedFrames; ++frame) {
            Player::frame(1.0 / 60.0);
        }

        const bool waited = device.getPresentedWaits() == static_cast<std::uint64_t>(kPlayedFrames);
        Player::cleanup();
        return waited ? Outcome::Clean : Outcome::DisplayNotWaited;
    }
};

// A player whose creation fails lets go of everything it made, its host with the window among them, and ends the services of the platform that it started, so the process ends without a player to clean up.
TEST_F(PlayerTest, LeavesNothingOfAPlayerWhoseCreationFailed) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(std::exit(static_cast<int>(createOnFailingPlatform())), ::testing::ExitedWithCode(static_cast<int>(Outcome::Clean)), "");
}

// Every platform reads the events of a frame after the frame before it returned, so each frame that presented ends with a wait for the display, and input reaches the screen as soon as the display allows.
TEST_F(PlayerTest, WaitsForTheDisplayAfterEveryFrameThatPresented) {
    GTEST_FLAG_SET(death_test_style, "threadsafe");
    EXPECT_EXIT(std::exit(static_cast<int>(playFrames())), ::testing::ExitedWithCode(static_cast<int>(Outcome::Clean)), "");
}

} // namespace haylen::platform
