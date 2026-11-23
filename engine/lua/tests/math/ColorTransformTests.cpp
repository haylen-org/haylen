#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Transform2D.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::math {

TEST(ColorTest, ParsesTiledNotations) {
    const auto opaque = haylen::math::Color::parse("#ff8000");
    ASSERT_TRUE(opaque.has_value());
    EXPECT_EQ(*opaque, haylen::math::Color::fromRgba8(255, 128, 0, 255));

    const auto translucent = haylen::math::Color::parse("#80ff0000");
    ASSERT_TRUE(translucent.has_value());
    EXPECT_EQ(*translucent, haylen::math::Color::fromRgba8(255, 0, 0, 128));

    EXPECT_EQ(haylen::math::Color::parse("00ff00"), haylen::math::Color::fromRgba8(0, 255, 0));
    EXPECT_FALSE(haylen::math::Color::parse("#12345").has_value());
    EXPECT_FALSE(haylen::math::Color::parse("#zzzzzz").has_value());
}

TEST(ColorTest, PackingAndHelpers) {
    EXPECT_EQ(haylen::math::Color::white().toRgba8(), 0xFFFFFFFFU);
    EXPECT_EQ(haylen::math::Color::black().toRgba8(), 0xFF000000U);
    EXPECT_EQ(haylen::math::Color::transparent().toRgba8(), 0x00000000U);
    EXPECT_EQ((haylen::math::Color{2.0F, -1.0F, 0.0F, 1.0F}).toRgba8(), 0xFF0000FFU);
    EXPECT_EQ(haylen::math::Color::fromHex(0x11223344U), haylen::math::Color::fromRgba8(0x11, 0x22, 0x33, 0x44));
    EXPECT_EQ(haylen::math::Color::white().withAlpha(0.5F).a, 0.5F);
    EXPECT_EQ((haylen::math::Color{1.0F, 0.5F, 0.2F, 0.5F}.getPremultiplied()), (haylen::math::Color{0.5F, 0.25F, 0.1F, 0.5F}));
    EXPECT_EQ(haylen::math::Color::white() * (haylen::math::Color{0.5F, 0.5F, 0.5F, 1.0F}), (haylen::math::Color{0.5F, 0.5F, 0.5F, 1.0F}));
    EXPECT_EQ(haylen::math::Color::lerp(haylen::math::Color::black(), haylen::math::Color::white(), 0.5F), (haylen::math::Color{0.5F, 0.5F, 0.5F, 1.0F}));
}

TEST(ColorTest, ConvertsEveryHueSector) {
    EXPECT_EQ(haylen::math::Color::fromHsv(0.0F, 1.0F, 1.0F), (haylen::math::Color{1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_EQ(haylen::math::Color::fromHsv(1.0F / 3.0F, 1.0F, 1.0F).g, 1.0F);
    EXPECT_EQ(haylen::math::Color::fromHsv(2.0F / 3.0F, 1.0F, 1.0F).b, 1.0F);

    for (int sector = 0; sector < 6; ++sector) {
        const float hue = (static_cast<float>(sector) + 0.5F) / 6.0F;
        const haylen::math::Color color = haylen::math::Color::fromHsv(hue, 1.0F, 1.0F, 0.25F);
        EXPECT_FLOAT_EQ(color.a, 0.25F);
        EXPECT_FLOAT_EQ(std::max({color.r, color.g, color.b}), 1.0F);
        EXPECT_NEAR(color.toHsv().hue, hue, 1e-5F) << sector;
    }
}

TEST(ColorTest, AHueThatIsNotFiniteGivesChannelsThatAreNotNumbers) {
    for (const float hue : {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        const haylen::math::Color color = haylen::math::Color::fromHsv(hue, 1.0F, 1.0F, 0.5F);
        EXPECT_TRUE(std::isnan(color.r) && std::isnan(color.g) && std::isnan(color.b));
        EXPECT_EQ(color.a, 0.5F);
    }
}

TEST(ColorLuaTest, ReadsLuminanceContrastAndTheContrastingColor) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math') night = m.color('#FF1E2A44')");
    EXPECT_EQ(fixture.lua("return tostring(night:dark()) .. ' ' .. night:contrastingColor():toHex() .. ' ' .. string.format('%.1f', m.contrast(m.black(), m.white()))"), "true #FFFFFFFF 21.0");
    EXPECT_EQ(fixture.lua("return string.format('%.3f', m.white():luminance())"), "1.000");
}

TEST(ColorLuaTest, ConvertsToHsvAndBlendsAroundTheHueCircle) {
    test::LuaFixture fixture;
    fixture.runLua("m = require('haylen.math')");

    EXPECT_EQ(fixture.lua("return string.format('%.3f %.1f %.1f %.2f', m.color(0, 0, 1, 0.25):toHsv())"), "0.667 1.0 1.0 0.25");
    // Red to blue passes through magenta in HSV instead of the dark purple of a plain lerp.
    EXPECT_EQ(fixture.lua("local red, blue = m.color('#FFFF0000'), m.color('#FF0000FF') return tostring(red:lerpHsv(blue, 0.5)) .. ' ' .. tostring(red:lerp(blue, 0.5))"), "#FFFF00FF #FF800080");
    // A gray takes the hue of the other color, so only the saturation changes, whichever end it is.
    EXPECT_EQ(fixture.lua("return tostring(m.color(1, 1, 1):lerpHsv({1, 0, 0}, 0.5))"), "#FFFF8080");
    EXPECT_EQ(fixture.lua("return tostring(m.color(1, 0, 0):lerpHsv({1, 1, 1}, 0.5))"), "#FFFF8080");
}

// The contrast rules of accessibility read luminance in linear light, and the text color of a background is the one of black and white with the higher contrast.
TEST(ColorTest, MeasuresLuminanceAndContrast) {
    EXPECT_FLOAT_EQ(haylen::math::Color::black().getLuminance(), 0.0F);
    EXPECT_FLOAT_EQ(haylen::math::Color::white().getLuminance(), 1.0F);
    EXPECT_NEAR(haylen::math::Color::fromHex(0x777777FFU).getLuminance(), 0.1845F, 0.001F);
    EXPECT_FLOAT_EQ(haylen::math::Color::contrast(haylen::math::Color::black(), haylen::math::Color::white()), 21.0F);
    EXPECT_FLOAT_EQ(haylen::math::Color::contrast(haylen::math::Color::white(), haylen::math::Color::black()), 21.0F);
    EXPECT_NEAR(haylen::math::Color::contrast(haylen::math::Color::fromHex(0x767676FFU), haylen::math::Color::white()), 4.54F, 0.01F);

    EXPECT_TRUE(haylen::math::Color::fromHex(0x1E2A44FFU).isDark());
    EXPECT_FALSE(haylen::math::Color::fromHex(0xF4EFE3FFU).isDark());
    EXPECT_FALSE(haylen::math::Color::fromHex(0xFFD54FFFU).isDark());
    EXPECT_EQ(haylen::math::Color::fromHex(0x1E2A4480U).getContrastingColor(), haylen::math::Color::white().withAlpha(haylen::math::Color::fromHex(0x1E2A4480U).a));
    EXPECT_EQ(haylen::math::Color::fromHex(0xFFD54FFFU).getContrastingColor(), haylen::math::Color::black());
}

TEST(Transform2DTest, ComposesAndInverts) {
    const haylen::math::Transform2D transform = haylen::math::Transform2D::compose(haylen::math::Vec2(10.0F, 20.0F), haylen::math::Math::kHalfPi, haylen::math::Vec2(2.0F, 2.0F));
    const haylen::math::Vec2 point = transform.apply(haylen::math::Vec2(1.0F, 0.0F));
    EXPECT_NEAR(point.x, 10.0F, 1e-5F);
    EXPECT_NEAR(point.y, 22.0F, 1e-5F);

    const haylen::math::Vec2 back = transform.getInverse().apply(point);
    EXPECT_NEAR(back.x, 1.0F, 1e-5F);
    EXPECT_NEAR(back.y, 0.0F, 1e-5F);

    const haylen::math::Transform2D combined = haylen::math::Transform2D::translation(haylen::math::Vec2(5.0F, 0.0F)) * haylen::math::Transform2D::scaling(haylen::math::Vec2(3.0F, 3.0F));
    EXPECT_EQ(combined.apply(haylen::math::Vec2(1.0F, 1.0F)), haylen::math::Vec2(8.0F, 3.0F));
    EXPECT_EQ(combined.applyVector(haylen::math::Vec2(1.0F, 1.0F)), haylen::math::Vec2(3.0F, 3.0F));
    EXPECT_EQ(combined.getTranslation(), haylen::math::Vec2(5.0F, 0.0F));
    EXPECT_FLOAT_EQ(combined.getDeterminant(), 9.0F);

    const haylen::math::Vec2 rotated = haylen::math::Transform2D::rotation(haylen::math::Math::kPi).apply(haylen::math::Vec2(1.0F, 0.0F));
    EXPECT_NEAR(rotated.x, -1.0F, 1e-6F);
    EXPECT_EQ(haylen::math::Transform2D::identity().apply(haylen::math::Vec2(4.0F, 5.0F)), haylen::math::Vec2(4.0F, 5.0F));
}

TEST(Transform2DTest, SingularInverseCollapsesToZero) {
    const haylen::math::Transform2D singular = haylen::math::Transform2D::scaling(haylen::math::Vec2(0.0F, 1.0F));
    EXPECT_EQ(singular.getInverse().apply(haylen::math::Vec2(3.0F, 4.0F)), haylen::math::Vec2{});
}

TEST(Transform2DTest, SkewShearsTheVerticalAxis) {
    const haylen::math::Transform2D skewed = haylen::math::Transform2D::compose(haylen::math::Vec2{}, 0.0F, haylen::math::Vec2(1.0F, 1.0F), haylen::math::Vec2(haylen::math::Math::kPi / 4.0F, 0.0F));
    const haylen::math::Vec2 top = skewed.apply(haylen::math::Vec2(0.0F, 1.0F));
    EXPECT_NEAR(top.x, -std::sin(haylen::math::Math::kPi / 4.0F), 1e-5F);
}

} // namespace haylen::lua::math
