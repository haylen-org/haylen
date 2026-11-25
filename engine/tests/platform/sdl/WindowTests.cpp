#include <gtest/gtest.h>

#include <SDL3/SDL_video.h>

#include "haylen/platform/Display.hpp"
#include "platform/sdl/Window.hpp"

namespace haylen::platform::sdl {

// SDL gives a mode in the points of its display with the density of its pixels, while the modes of a display count pixels, so a mode of a display at twice the density lists twice the size that SDL gives, as the modes that fullscreen matches against do.
TEST(WindowTest, DescribesFullscreenModesInPixels) {
    SDL_DisplayMode mode{};
    mode.w = 1920;
    mode.h = 1080;
    mode.pixel_density = 2.0F;
    mode.refresh_rate = 120.0F;

    const Display::Mode described = Window::describeMode(mode);
    EXPECT_EQ(described.width, 3840);
    EXPECT_EQ(described.height, 2160);
    EXPECT_FLOAT_EQ(described.refreshRate, 120.0F);
    EXPECT_FLOAT_EQ(described.scale, 2.0F);

    mode.pixel_density = 1.25F;
    EXPECT_EQ(Window::describeMode(mode).width, 2400);
    EXPECT_EQ(Window::describeMode(mode).height, 1350);
}

} // namespace haylen::platform::sdl
