#include <gtest/gtest.h>

#include <deque>
#include <string>
#include <vector>

#include "haylen/platform/SystemAnswer.hpp"
#include "haylen/platform/Window.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// The mode of the mouse over the window changes only once the platform grants it, so a mode that a browser or an iPad refuses, or grants after a newer one, never reports a mode that the pointer is not in.
class MouseModeTest : public ::testing::Test {
  protected:
    // Asks the window for the mode and returns where its answer lands, which records the state of each answer.
    std::vector<SystemAnswer::State>& request(Window::MouseMode mode) {
        std::vector<SystemAnswer::State>& states = answers.emplace_back();
        window().setMouseMode(mode, [&states](SystemAnswer answer) { states.push_back(answer.state); });
        return states;
    }

    [[nodiscard]] Window& window() noexcept {
        return fixture.host();
    }

    test::EngineFixture fixture;
    // A deque keeps where every answer lands while later requests add theirs.
    std::deque<std::vector<SystemAnswer::State>> answers;
};

TEST_F(MouseModeTest, TakesEveryModeThatThePlatformGrants) {
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Normal);

    for (const Window::MouseMode mode : {Window::MouseMode::Hidden, Window::MouseMode::Locked, Window::MouseMode::Confined, Window::MouseMode::ConfinedHidden, Window::MouseMode::Normal}) {
        const std::vector<SystemAnswer::State>& states = request(mode);
        ASSERT_EQ(states.size(), 1U);
        EXPECT_EQ(states.front(), SystemAnswer::State::Granted);
        EXPECT_EQ(window().getMouseMode(), mode);
    }
}

TEST_F(MouseModeTest, KeepsTheModeBeforeARequestThatThePlatformRefuses) {
    ASSERT_EQ(request(Window::MouseMode::Hidden).size(), 1U);

    fixture.host().setWindowAnswer({.state = SystemAnswer::State::Denied, .message = "The browser locks the pointer only during a click or a key of the person."});
    const std::vector<SystemAnswer::State>& states = request(Window::MouseMode::Locked);
    ASSERT_EQ(states.size(), 1U);
    EXPECT_EQ(states.front(), SystemAnswer::State::Denied);
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Hidden);
}

TEST_F(MouseModeTest, LeavesTheModeToANewerRequestThatThePlatformGrantedFirst) {
    fixture.host().holdMouseModeAnswer();
    const std::vector<SystemAnswer::State>& lock = request(Window::MouseMode::Locked);
    EXPECT_TRUE(lock.empty());

    ASSERT_EQ(request(Window::MouseMode::Hidden).size(), 1U);
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Hidden);

    fixture.host().answerHeldMouseMode({.state = SystemAnswer::State::Granted});
    ASSERT_EQ(lock.size(), 1U);
    EXPECT_EQ(lock.front(), SystemAnswer::State::Granted);
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Hidden);
}

TEST_F(MouseModeTest, TakesTheModeOfALateGrantWhenTheNewerRequestWasRefused) {
    fixture.host().holdMouseModeAnswer();
    const std::vector<SystemAnswer::State>& lock = request(Window::MouseMode::Locked);

    fixture.host().setWindowAnswer({.state = SystemAnswer::State::Unsupported, .message = "A browser keeps no pointer inside a page."});
    ASSERT_EQ(request(Window::MouseMode::Confined).size(), 1U);
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Normal);

    fixture.host().answerHeldMouseMode({.state = SystemAnswer::State::Granted});
    ASSERT_EQ(lock.size(), 1U);
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Locked);
}

TEST_F(MouseModeTest, FreesTheMouseForTheErrorScreen) {
    ASSERT_EQ(request(Window::MouseMode::ConfinedHidden).size(), 1U);

    fixture.engine().reportError(std::string("The app failed."));
    EXPECT_EQ(window().getMouseMode(), Window::MouseMode::Normal);
}

} // namespace haylen::platform
