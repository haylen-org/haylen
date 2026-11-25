#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/io/Package.hpp"
#include "haylen/platform/Bridge.hpp"
#include "haylen/port/Services.hpp"
#include "platform/BridgeRelay.hpp"
#include "platform/Services.hpp"

namespace haylen::platform {

// A platform with nothing but its name and one screen, which every port must give.
class MinimalServices final : public port::PlatformService, public port::DisplayService {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "minimal";
    }

    void initialize() override {}

    void shutdown() noexcept override {}

    [[nodiscard]] std::shared_ptr<io::Package> openBundledPackage() override {
        throw std::runtime_error("The minimal platform ships no package.");
    }

    [[nodiscard]] std::filesystem::path getUserDataDirectory(std::string_view identifier) override {
        return std::filesystem::path("data") / std::string(identifier);
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
        return {0.0F, 0.0F, 1920.0F, 1080.0F};
    }

    [[nodiscard]] static port::Services create() {
        const auto services = std::make_shared<MinimalServices>();
        return {.platform = services, .displays = services};
    }

    // The services belong to the process, so the tests that run in one process install them once.
    static void installOnce() {
        static std::once_flag installed;
        std::call_once(installed, &MinimalServices::install);
    }

    static void install() {
        Services::install(create());
    }
};

TEST(ServicesTest, RefusesAPlatformWithoutItsDisplays) {
    port::Services services = MinimalServices::create();
    services.displays = nullptr;

    EXPECT_THROW(Services::install(std::move(services)), std::invalid_argument);
}

TEST(ServicesTest, InstallsTheServicesOfAProcessOnce) {
    MinimalServices::installOnce();

    EXPECT_EQ(Services::getName(), "minimal");
    EXPECT_THROW(Services::install(MinimalServices::create()), std::invalid_argument);
}

// Each capability that the platform did not register answers the way its service describes, never as an empty implementation that seems to have worked.
TEST(ServicesTest, AnswersForEveryCapabilityThePlatformLacks) {
    MinimalServices::installOnce();

    EXPECT_FALSE(Services::hasDesktop());
    EXPECT_EQ(Services::getNativeWindow().handle, nullptr);
    EXPECT_EQ(Services::getWindowFrame().width, 1920.0F);
    EXPECT_EQ(Services::getSafeAreaInsets().top, 0.0F);
    EXPECT_EQ(Services::getOrientation(), Orientation::Landscape);
    EXPECT_EQ(Services::getStatusBarHeight(), 0.0F);
    EXPECT_FALSE(Services::getFold());
    std::vector<math::Rect> cutouts{{1.0F, 1.0F, 1.0F, 1.0F}};
    Services::getCutouts(cutouts);
    EXPECT_TRUE(cutouts.empty());
    EXPECT_TRUE(Services::getNativePlugins().empty());
    EXPECT_FALSE(Services::forwardLink("com.haylen.minimal", "minimal://open"));
    EXPECT_TRUE(Services::isBackgroundAudioDeclared());
    EXPECT_FALSE(Services::isAccessibilityTreeWanted());
    EXPECT_EQ(Services::openVideo(nullptr, "intro.mp4"), nullptr);
    EXPECT_FALSE(Services::findSystemFont(U'A'));
    EXPECT_THROW((void)Services::readSystemFont({.name = "Sans"}), std::logic_error);
    EXPECT_TRUE(Services::explainBlockedSocket(3).empty());
    EXPECT_TRUE(Services::getNetworkRequirement().empty());
}

TEST(ServicesTest, FailsACallThatNoNativePartOfThePlatformHandles) {
    MinimalServices::installOnce();
    Bridge bridge([](std::uint64_t id, std::string_view method, std::string_view params, std::span<const std::vector<std::byte>> buffers, std::span<const varn::dataview::DataView> views) { Services::dispatch(id, method, params, buffers, views); }, [](std::uint64_t, std::string_view) {});
    BridgeRelay::attach(bridge);

    std::optional<Bridge::Result> result;
    (void)bridge.call("store.purchase", {.json = core::Json::object()}, [&result](Bridge::Result value) { result = std::move(value); });
    bridge.pump();
    BridgeRelay::detach(bridge);

    ASSERT_TRUE(result);
    EXPECT_FALSE(result->ok);
    EXPECT_EQ(result->error.code, "noHandler");
    EXPECT_EQ(result->error.message, "No native handler is registered for \"store.purchase\".");
}

} // namespace haylen::platform
