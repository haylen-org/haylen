#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_video.h>

#include "platform/sdl/WaylandDisplays.hpp"

namespace haylen::platform::sdl {

// A display of a Wayland session takes the name of its output as its id, which stays with the connector of its monitor, so two monitors of one model, whose descriptions are the same, keep ids of their own.
TEST(WaylandDisplaysTest, TakesTheNameOfTheOutputAsTheIdOfADisplay) {
    const SDL_PropertiesID left = SDL_CreateProperties();
    const SDL_PropertiesID right = SDL_CreateProperties();
    ASSERT_NE(left, 0U) << SDL_GetError();
    ASSERT_NE(right, 0U) << SDL_GetError();
    ASSERT_TRUE(SDL_SetStringProperty(left, SDL_PROP_DISPLAY_WAYLAND_WL_OUTPUT_NAME_STRING, "DP-1"));
    ASSERT_TRUE(SDL_SetStringProperty(right, SDL_PROP_DISPLAY_WAYLAND_WL_OUTPUT_NAME_STRING, "DP-2"));

    EXPECT_EQ(WaylandDisplays::readId(left), "DP-1");
    EXPECT_EQ(WaylandDisplays::readId(right), "DP-2");
    SDL_DestroyProperties(left);
    SDL_DestroyProperties(right);
}

// SDL keeps the scale of a Wayland output in the pixel density of its desktop mode and gives no content scale to a display, so an output at twice the density reads a scale of 2.
TEST(WaylandDisplaysTest, ReadsTheScaleOfAnOutputFromItsDesktopMode) {
    SDL_DisplayMode desktop{};
    desktop.w = 960;
    desktop.h = 540;
    desktop.pixel_density = 2.0F;
    EXPECT_FLOAT_EQ(WaylandDisplays::readScale(desktop), 2.0F);

    desktop.pixel_density = 1.5F;
    EXPECT_FLOAT_EQ(WaylandDisplays::readScale(desktop), 1.5F);
}

// A compositor that names none of its outputs leaves the displays without ids that tell them apart, which the app hears as an error.
TEST(WaylandDisplaysTest, RefusesADisplayWhoseOutputHasNoName) {
    const SDL_PropertiesID unnamed = SDL_CreateProperties();
    ASSERT_NE(unnamed, 0U) << SDL_GetError();
    std::string error;
    try {
        static_cast<void>(WaylandDisplays::readId(unnamed));
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }

    EXPECT_NE(error.find("names none of its outputs"), std::string::npos) << error;
    SDL_DestroyProperties(unnamed);
}

} // namespace haylen::platform::sdl
