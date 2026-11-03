#include <gtest/gtest.h>

#include <optional>
#include <string>

#include "gpu/NullDevice.hpp"
#include "platform/DeviceRecovery.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// The first loss of the device starts the app again on a new device, which draws on, and a loss of that replacement ends the app with the reason, which names the graphics API.
TEST(DeviceRecoveryTest, ReplacesALostDeviceOnceAndEndsTheAppWhenTheReplacementIsLost) {
    test::EngineFixture fixture;
    DeviceRecovery recovery;
    fixture.frames(1);
    fixture.host().getGpu().lose();
    ASSERT_TRUE(fixture.host().getGpu().isLost());
    EXPECT_EQ(recovery.recordLoss(fixture.host().getGpu().getApi()), std::nullopt);

    fixture.restart();
    fixture.frames(1);
    gpu::NullDevice& replacement = fixture.host().getGpu();
    EXPECT_FALSE(replacement.isLost());
    EXPECT_FALSE(replacement.getLastFrame().passes.empty());

    replacement.lose();
    EXPECT_EQ(recovery.recordLoss(replacement.getApi()), std::optional<std::string>("The GPU device of \"none\" stopped working again after it replaced a lost one, so the app cannot draw."));
}

} // namespace haylen::platform
