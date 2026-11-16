#include <gtest/gtest.h>

#include "platform/worker/Keys.hpp"

namespace haylen::platform::worker {

TEST(WorkerKeysTest, NamesKeysByTheirPlaceOnTheKeyboard) {
    EXPECT_EQ(Keys::fromCode("KeyA"), input::Key::A);
    EXPECT_EQ(Keys::fromCode("Digit7"), input::Key::Digit7);
    EXPECT_EQ(Keys::fromCode("ArrowLeft"), input::Key::Left);
    EXPECT_EQ(Keys::fromCode("NumpadEnter"), input::Key::KeypadEnter);
    EXPECT_EQ(Keys::fromCode("MetaRight"), input::Key::RightSuper);
    EXPECT_EQ(Keys::fromCode("IntlBackslash"), input::Key::World2);
}

TEST(WorkerKeysTest, LeavesKeysOutsideTheEngineUnknown) {
    EXPECT_EQ(Keys::fromCode("MediaPlayPause"), input::Key::Unknown);
    EXPECT_EQ(Keys::fromCode(""), input::Key::Unknown);
    EXPECT_EQ(Keys::fromCode("keya"), input::Key::Unknown);
}

} // namespace haylen::platform::worker
