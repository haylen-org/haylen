#include <gtest/gtest.h>

#include "platform/android/AndroidKeyClaims.hpp"

namespace haylen::platform {

using Action = AndroidKeyClaims::Action;

TEST(AndroidKeyClaimsTest, ThePressDecidesForItsRepeatsAndItsRelease) {
    AndroidKeyClaims claims;
    EXPECT_TRUE(claims.take(29, Action::Press, true));
    EXPECT_TRUE(claims.take(29, Action::Repeat, false));
    EXPECT_TRUE(claims.take(29, Action::Release, false));

    // A press that went to a view, such as a focused text field, keeps its release from the app.
    EXPECT_FALSE(claims.take(29, Action::Press, false));
    EXPECT_FALSE(claims.take(29, Action::Repeat, true));
    EXPECT_FALSE(claims.take(29, Action::Release, true));
}

TEST(AndroidKeyClaimsTest, AReleaseWithoutAPressGoesToTheViews) {
    AndroidKeyClaims claims;
    EXPECT_TRUE(claims.take(62, Action::Press, true));
    EXPECT_TRUE(claims.take(62, Action::Release, true));
    EXPECT_FALSE(claims.take(62, Action::Release, true));
}

TEST(AndroidKeyClaimsTest, KeyCodesOutsideTheRangeOfAndroidGoToTheViews) {
    AndroidKeyClaims claims;
    EXPECT_FALSE(claims.take(-1, Action::Press, true));
    EXPECT_FALSE(claims.take(AndroidKeyClaims::kKeyCodes, Action::Press, true));
}

} // namespace haylen::platform
