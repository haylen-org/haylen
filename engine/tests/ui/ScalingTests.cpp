#include <gtest/gtest.h>

#include "haylen/ui/Scaling.hpp"

namespace haylen::ui {

TEST(ScalingTest, ResolvesTheDesignUnitsOfAUiUnit) {
    Scaling scaling;
    EXPECT_FLOAT_EQ(scaling.resolve(0.5F, 3.0F, {1920.0F, 1080.0F}), 1.0F);
    scaling.factor = 1.5F;
    EXPECT_FLOAT_EQ(scaling.resolve(0.5F, 3.0F, {1920.0F, 1080.0F}), 1.5F);

    // A UI unit spans three quarters of a point, so the denser the screen in points the more design units it takes, until the shorter side keeps 480 units.
    scaling = {.mode = Scaling::Mode::Physical, .factor = 1.0F};
    EXPECT_FLOAT_EQ(scaling.resolve(1.0F, 1.0F, {1920.0F, 1080.0F}), 0.75F);
    EXPECT_FLOAT_EQ(scaling.resolve(2.0F, 2.0F, {1920.0F, 1080.0F}), 0.75F);
    EXPECT_FLOAT_EQ(scaling.resolve(1.0F, 2.0F, {1920.0F, 1080.0F}), 1.5F);
    EXPECT_FLOAT_EQ(scaling.resolve(0.5F, 3.0F, {2400.0F, 1080.0F}), 2.25F);
    EXPECT_EQ(Scaling::modeFromName("physical"), Scaling::Mode::Physical);
    EXPECT_EQ(Scaling::modeName(Scaling::Mode::Design), "design");
    EXPECT_FALSE(Scaling::modeFromName("huge").has_value());
}

} // namespace haylen::ui
