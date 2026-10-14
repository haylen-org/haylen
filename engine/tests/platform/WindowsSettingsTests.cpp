#include <gtest/gtest.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "platform/windows/WindowsSettings.hpp"

namespace haylen::platform {

// The settings of the user who runs the tests are whatever they are, so the test checks that the reader turns them into values the engine accepts and agrees with what Windows answers directly.
TEST(WindowsSettingsTest, ReadsTheAccessibilitySettingsOfTheUser) {
    const AccessibilitySettings settings = WindowsSettings::readAccessibility();
    EXPECT_GE(settings.textScale, 1.0F);
    EXPECT_LE(settings.textScale, 2.25F);

    BOOL animations = TRUE;
    ASSERT_NE(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0), FALSE);
    EXPECT_EQ(settings.reducedMotion, animations == FALSE);
    HIGHCONTRASTW contrast{.cbSize = static_cast<UINT>(sizeof(HIGHCONTRASTW))};
    ASSERT_NE(SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0), FALSE);
    EXPECT_EQ(settings.highContrast, (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0);
}

} // namespace haylen::platform
