#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

#include "haylen/core/Engine.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/math/Vec2.hpp"
#include "platform/sdl/Events.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform::sdl {

// A long session of mouse motion that a thread of the window system keeps sending, as a person who moves the pointer inside the window does, reaches the engine through the event queue of SDL and the translation of the runtime in every frame, with no pause.
class MouseMotionTest : public ::testing::Test {
  protected:
    static constexpr std::chrono::seconds kSession{4};
    static constexpr std::chrono::milliseconds kMotionInterval{1};
    static constexpr std::chrono::milliseconds kFrameInterval{8};
    // A frame that comes this long after the one before, or a motion that waits this long in the queue, is a pause a person notices.
    static constexpr double kPauseSeconds = 0.25;

    void SetUp() override {
        ASSERT_TRUE(SDL_Init(SDL_INIT_EVENTS)) << SDL_GetError();
    }

    void TearDown() override {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }

    // Sends the motion of a pointer that sweeps across the window, each one a pixel on from the one before.
    void move(std::atomic<bool>& moving, std::atomic<int>& sent) {
        auto next = std::chrono::steady_clock::now();
        for (int step = 0; moving.load(); ++step) {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.x = static_cast<float>(step % 1000);
            event.motion.y = 200.0F;
            event.motion.xrel = 1.0F;
            ASSERT_TRUE(SDL_PushEvent(&event)) << SDL_GetError();
            sent.fetch_add(1);
            next += kMotionInterval;
            std::this_thread::sleep_until(next);
        }
    }

    [[nodiscard]] static double getSeconds(Uint64 nanoseconds) noexcept {
        return static_cast<double>(nanoseconds) / 1.0e9;
    }

    test::EngineFixture fixture;
    Events events;
    std::vector<platform::Event> translated;
};

TEST_F(MouseMotionTest, ReachesEveryFrameWhileThePointerStaysInTheWindow) {
    std::atomic<bool> moving{true};
    std::atomic<int> sent{0};
    std::thread pointer([&] { move(moving, sent); });

    const input::Input& input = fixture.engine().getInput();
    const auto end = std::chrono::steady_clock::now() + kSession;
    int received = 0;
    int movedFrames = 0;
    double longestWait = 0.0;
    double longestFrame = 0.0;
    Uint64 previousFrame = SDL_GetTicksNS();
    while (std::chrono::steady_clock::now() < end) {
        const Uint64 frameStart = SDL_GetTicksNS();
        const double frameSeconds = getSeconds(frameStart - previousFrame);
        longestFrame = std::max(longestFrame, frameSeconds);
        previousFrame = frameStart;

        std::optional<math::Vec2> last;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ASSERT_EQ(event.type, SDL_EVENT_MOUSE_MOTION);
            longestWait = std::max(longestWait, getSeconds(SDL_GetTicksNS() - event.motion.timestamp));
            translated.clear();
            events.translate(event, {.framebuffer = {1000.0F, 400.0F}}, translated);
            ASSERT_EQ(translated.size(), 1U);
            fixture.engine().handleEvent(translated[0]);
            last = math::Vec2{event.motion.x, event.motion.y};
            ++received;
        }

        fixture.frames(1, frameSeconds);
        if (last) {
            ++movedFrames;
            EXPECT_EQ(input.getMouseFramebufferPosition(), *last);
            EXPECT_TRUE(input.isMouseInside());
        }

        std::this_thread::sleep_for(kFrameInterval);
    }

    moving = false;
    pointer.join();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ++received;
    }

    EXPECT_EQ(received, sent.load());
    EXPECT_GT(movedFrames, 0);
    EXPECT_LT(longestWait, kPauseSeconds);
    EXPECT_LT(longestFrame, kPauseSeconds);
}

// Once the pointer leaves the window, the window system sends it no motion until it comes back, so the app keeps the last position it had inside.
TEST_F(MouseMotionTest, KeepsTheLastPositionOnceThePointerLeavesTheWindow) {
    SDL_Event motion{};
    motion.type = SDL_EVENT_MOUSE_MOTION;
    motion.motion.x = 640.0F;
    motion.motion.y = 200.0F;
    events.translate(motion, {.framebuffer = {1000.0F, 400.0F}}, translated);
    SDL_Event leave{};
    leave.type = SDL_EVENT_WINDOW_MOUSE_LEAVE;
    events.translate(leave, {.framebuffer = {1000.0F, 400.0F}}, translated);
    ASSERT_EQ(translated.size(), 2U);
    for (const platform::Event& event : translated) {
        fixture.engine().handleEvent(event);
    }

    fixture.frames(30);
    const input::Input& input = fixture.engine().getInput();
    EXPECT_FALSE(input.isMouseInside());
    EXPECT_EQ(input.getMouseFramebufferPosition(), (math::Vec2{640.0F, 200.0F}));
}

} // namespace haylen::platform::sdl
