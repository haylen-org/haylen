#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "platform/sdl/FullscreenRequest.hpp"

namespace haylen::platform::sdl {

class FullscreenRequestTest : public ::testing::Test {
  protected:
    // Records each answer that a request receives, in order.
    FullscreenRequest::Callback record() {
        return [this](Permission::Answer answer) { answers.push_back(std::move(answer)); };
    }

    std::vector<Permission::Answer> answers;
};

TEST_F(FullscreenRequestTest, AnswersOnceTheWindowReportsTheModeItAskedFor) {
    FullscreenRequest request;
    request.begin(true, record());
    EXPECT_TRUE(request.isPending());
    EXPECT_TRUE(request.isEntering());

    request.reach(false);
    EXPECT_TRUE(answers.empty());

    request.reach(true);
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers[0].state, Permission::State::Granted);
    EXPECT_FALSE(request.isPending());

    request.reach(true);
    EXPECT_EQ(answers.size(), 1U);
}

TEST_F(FullscreenRequestTest, DeniesTheRequestThatANewerOneReplaces) {
    FullscreenRequest request;
    request.begin(true, record());
    request.begin(false, record());
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers[0].state, Permission::State::Denied);
    EXPECT_EQ(answers[0].message, "A newer change of fullscreen replaced this one before the window finished it.");
    EXPECT_FALSE(request.isEntering());

    request.reach(true);
    EXPECT_EQ(answers.size(), 1U);
    request.reach(false);
    ASSERT_EQ(answers.size(), 2U);
    EXPECT_EQ(answers[1].state, Permission::State::Granted);
}

TEST_F(FullscreenRequestTest, AnswersAFailureOfTheSystem) {
    FullscreenRequest request;
    request.begin(true, record());
    request.fail({.state = Permission::State::Unsupported, .message = "The window could not go fullscreen."});
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers[0].state, Permission::State::Unsupported);
    EXPECT_FALSE(request.isPending());
}

TEST_F(FullscreenRequestTest, DeniesTheRequestThatWaitsWhenTheWindowCloses) {
    {
        FullscreenRequest request;
        request.begin(false, record());
    }
    ASSERT_EQ(answers.size(), 1U);
    EXPECT_EQ(answers[0].state, Permission::State::Denied);
    EXPECT_EQ(answers[0].message, "The window closed before it finished the change of fullscreen.");
}

} // namespace haylen::platform::sdl
