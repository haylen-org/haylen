#include <gtest/gtest.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>

#include "haylen/core/Log.hpp"
#include "haylen/platform/Event.hpp"
#include "platform/sdl/PostedEvents.hpp"

namespace haylen::platform::sdl {

// The events that the platform posts wait in the queue of SDL behind their markers, which SDL refuses once its queue is full.
class PostedEventsTest : public ::testing::Test {
  protected:
    // More events than the queue of SDL holds.
    static constexpr int kMostPosts = 1 << 17;

    void SetUp() override {
        ASSERT_TRUE(SDL_Init(SDL_INIT_EVENTS)) << SDL_GetError();
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        posted.start();
        // clang-format off
        listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) {
            if (level == core::Log::Level::Error && line.find("the queue of the events of the window is full") != std::string_view::npos) {
                refusals.fetch_add(1);
            }
        });
        // clang-format on
    }

    void TearDown() override {
        core::Log::removeListener(listener);
        SDL_FlushEvents(SDL_EVENT_FIRST, SDL_EVENT_LAST);
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }

    PostedEvents posted;
    std::atomic<int> refusals = 0;
    std::uint64_t listener = 0;
};

// The event whose marker the full queue refuses goes back with an error in the log, so every marker in the queue hands over the event that was posted with it and no event waits without a marker.
TEST_F(PostedEventsTest, TakesBackAnEventWhoseMarkerTheFullQueueRefuses) {
    for (int index = 0; index < kMostPosts && refusals.load() == 0; ++index) {
        posted.post({.type = Event::Type::MouseScroll, .scroll = {static_cast<float>(index), 0.0F}});
    }

    ASSERT_EQ(refusals.load(), 1);
    const std::size_t waiting = posted.getWaiting();

    SDL_Event marker{};
    std::size_t taken = 0;
    while (SDL_PollEvent(&marker)) {
        if (!posted.isMarker(marker)) {
            continue;
        }

        const Event event = posted.take();
        EXPECT_EQ(event.scroll.x, static_cast<float>(taken));
        ++taken;
    }

    EXPECT_EQ(taken, waiting);
    EXPECT_EQ(posted.getWaiting(), 0U);
}

} // namespace haylen::platform::sdl
