#include <gtest/gtest.h>

#include <SDL3/SDL_events.h>

#include "platform/sdl/OutputPlacement.hpp"

namespace haylen::platform::sdl {

// The window of an output that covers its display reaches its place only when SDL reports it in fullscreen, whatever the window system sent before, so its output announces the size and the scale it has there.
class OutputPlacementTest : public ::testing::Test {
  protected:
    [[nodiscard]] static SDL_Event makeEvent(SDL_EventType type) noexcept {
        SDL_Event event{};
        event.type = type;
        return event;
    }
};

TEST_F(OutputPlacementTest, PlacesAWindowThatCoversItsDisplayAtTheEnterFullscreenEvent) {
    OutputPlacement placement(true);
    EXPECT_FALSE(placement.isPlaced());

    // A window system that shows the window, sizes it and moves it to another scale before fullscreen leaves it out of place.
    EXPECT_FALSE(placement.take(makeEvent(SDL_EVENT_WINDOW_SHOWN)));
    EXPECT_FALSE(placement.take(makeEvent(SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)));
    EXPECT_FALSE(placement.take(makeEvent(SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)));
    EXPECT_FALSE(placement.isPlaced());

    EXPECT_TRUE(placement.take(makeEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)));
    EXPECT_TRUE(placement.isPlaced());
    EXPECT_FALSE(placement.take(makeEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)));
}

TEST_F(OutputPlacementTest, PlacesAWindowOfTheChosenSizeAtOnce) {
    OutputPlacement placement(false);
    EXPECT_TRUE(placement.isPlaced());
    EXPECT_FALSE(placement.take(makeEvent(SDL_EVENT_WINDOW_ENTER_FULLSCREEN)));
}

} // namespace haylen::platform::sdl
