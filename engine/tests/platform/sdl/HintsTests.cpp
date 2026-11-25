#include <gtest/gtest.h>

#include <string_view>

#include <SDL3/SDL_hints.h>

#include "haylen/core/AppConfig.hpp"
#include "platform/sdl/Hints.hpp"

namespace haylen::platform::sdl {

// A pen reaches the app as a touch, as a finger does, so SDL makes no events of the mouse from either one and the app never hears a pen twice.
TEST(HintsTest, KeepsThePenAndTheFingersFromTheMouse) {
    Hints::apply(core::AppConfig{});

    EXPECT_EQ(std::string_view(SDL_GetHint(SDL_HINT_PEN_MOUSE_EVENTS)), "0");
    EXPECT_EQ(std::string_view(SDL_GetHint(SDL_HINT_TOUCH_MOUSE_EVENTS)), "0");
    EXPECT_EQ(SDL_GetHint(SDL_HINT_PEN_TOUCH_EVENTS), nullptr);
    SDL_ResetHints();
}

} // namespace haylen::platform::sdl
