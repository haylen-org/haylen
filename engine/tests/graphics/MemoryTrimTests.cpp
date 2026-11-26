#include <gtest/gtest.h>

#include <optional>
#include <vector>

#include "gpu/NullDevice.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/gpu/ContractError.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/platform/Event.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics {

// The engine gives back the GPU memory that the device holds for later once a burst of loads and releases is over and when the system runs low on memory, and steady frames neither trim nor allocate.
class MemoryTrimTest : public ::testing::Test {
  protected:
    static constexpr double kFrameSeconds = 1.0 / 60.0;

    MemoryTrimTest() {
        app.frames(static_cast<int>(Device::kTrimDelay / kFrameSeconds) * 2);
    }

    [[nodiscard]] gpu::NullDevice& gpu() noexcept {
        return app.host().getGpu();
    }

    // Loads textures and lets them go, so the device holds their memory for later.
    void loadAndRelease() {
        std::optional<std::vector<Texture>> textures(std::in_place);
        for (int index = 0; index < 8; ++index) {
            textures->push_back(app.engine().getGraphics().createTexture(64, 64, math::Color::white()));
        }

        app.frames(1);
        textures.reset();
        app.frames(1);
    }

    test::EngineFixture app;
};

TEST_F(MemoryTrimTest, TrimsOnceTheSpareMemoryStoppedGrowing) {
    const int trims = gpu().getTrimCount();
    loadAndRelease();
    EXPECT_GT(gpu().getSpareBytes(), 0U);

    app.frames(static_cast<int>(Device::kTrimDelay / kFrameSeconds) - 10);
    EXPECT_EQ(gpu().getTrimCount(), trims);

    app.frames(20);
    EXPECT_EQ(gpu().getTrimCount(), trims + 1);
    EXPECT_EQ(gpu().getSpareBytes(), 0U);

    app.frames(static_cast<int>(Device::kTrimDelay / kFrameSeconds) * 2);
    EXPECT_EQ(gpu().getTrimCount(), trims + 1);
}

TEST_F(MemoryTrimTest, TrimsAtOnceWhenTheSystemRunsLowOnMemory) {
    const int trims = gpu().getTrimCount();
    loadAndRelease();
    app.engine().handleEvent({.type = platform::Event::Type::LowMemory, .memoryLevel = platform::Event::MemoryLevel::Warning});
    EXPECT_EQ(gpu().getTrimCount(), trims + 1);
    EXPECT_EQ(gpu().getSpareBytes(), 0U);
    EXPECT_EQ(app.engine().getGraphics().getSpareBytes(), 0U);
}

TEST_F(MemoryTrimTest, FollowsSteadyFramesWithoutTrimmingOrAllocating) {
    const int trims = gpu().getTrimCount();
    Device& graphics = app.engine().getGraphics();
    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 600; ++frame) {
        graphics.followSpareMemory(kFrameSeconds);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(gpu().getTrimCount(), trims);
}

// Only a bug of the engine trims inside a pass, which the device refuses.
TEST_F(MemoryTrimTest, RefusesATrimInsideAPass) {
    gpu::NullDevice& device = gpu();
    const gpu::Device::Image target = device.makeImage({.width = 4, .height = 4, .kind = gpu::Device::ImageKind::RenderTarget});
    const gpu::Device::ColorAttachment color{.image = target};
    device.beginPass({.colors = {&color, 1}});
    try {
        device.trim();
        ADD_FAILURE() << "The device trimmed inside a pass.";
    } catch (const gpu::ContractError& error) {
        EXPECT_EQ(error.getRule(), gpu::ContractError::Rule::PassOrder);
    }

    device.endPass();
    device.commit();
    device.destroy(target);
}

} // namespace haylen::graphics
