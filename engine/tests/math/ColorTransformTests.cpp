#include <gtest/gtest.h>

#include "haylen/math/Color.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Transform2D.hpp"

namespace haylen::math {

TEST(ColorTest, ParsesTiledNotations) {
    const auto opaque = Color::parse("#ff8000");
    ASSERT_TRUE(opaque.has_value());
    EXPECT_EQ(*opaque, Color::fromRgba8(255, 128, 0, 255));

    const auto translucent = Color::parse("#80ff0000");
    ASSERT_TRUE(translucent.has_value());
    EXPECT_EQ(*translucent, Color::fromRgba8(255, 0, 0, 128));

    EXPECT_EQ(Color::parse("00ff00"), Color::fromRgba8(0, 255, 0));
    EXPECT_FALSE(Color::parse("#12345").has_value());
    EXPECT_FALSE(Color::parse("#zzzzzz").has_value());
}

TEST(ColorTest, PackingAndHelpers) {
    EXPECT_EQ(Color::white().toRgba8(), 0xFFFFFFFFU);
    EXPECT_EQ(Color::black().toRgba8(), 0xFF000000U);
    EXPECT_EQ(Color::transparent().toRgba8(), 0x00000000U);
    EXPECT_EQ((Color{2.0F, -1.0F, 0.0F, 1.0F}).toRgba8(), 0xFF0000FFU);
    EXPECT_EQ(Color::fromHex(0x11223344U), Color::fromRgba8(0x11, 0x22, 0x33, 0x44));
    EXPECT_EQ(Color::white().withAlpha(0.5F).a, 0.5F);
    EXPECT_EQ((Color{1.0F, 0.5F, 0.2F, 0.5F}.getPremultiplied()), (Color{0.5F, 0.25F, 0.1F, 0.5F}));
    EXPECT_EQ(Color::white() * (Color{0.5F, 0.5F, 0.5F, 1.0F}), (Color{0.5F, 0.5F, 0.5F, 1.0F}));
    EXPECT_EQ(Color::lerp(Color::black(), Color::white(), 0.5F), (Color{0.5F, 0.5F, 0.5F, 1.0F}));
}

TEST(ColorTest, ConvertsEveryHueSector) {
    EXPECT_EQ(Color::fromHsv(0.0F, 1.0F, 1.0F), (Color{1.0F, 0.0F, 0.0F, 1.0F}));
    EXPECT_EQ(Color::fromHsv(1.0F / 3.0F, 1.0F, 1.0F).g, 1.0F);
    EXPECT_EQ(Color::fromHsv(2.0F / 3.0F, 1.0F, 1.0F).b, 1.0F);
    for (int sector = 0; sector < 6; ++sector) {
        const Color color = Color::fromHsv((static_cast<float>(sector) + 0.5F) / 6.0F, 1.0F, 1.0F, 0.25F);
        EXPECT_FLOAT_EQ(color.a, 0.25F);
        EXPECT_FLOAT_EQ(std::max({color.r, color.g, color.b}), 1.0F);
    }
}

TEST(Transform2DTest, ComposesAndInverts) {
    const Transform2D transform = Transform2D::compose(Vec2(10.0F, 20.0F), Math::kHalfPi, Vec2(2.0F, 2.0F));
    const Vec2 point = transform.apply(Vec2(1.0F, 0.0F));
    EXPECT_NEAR(point.x, 10.0F, 1e-5F);
    EXPECT_NEAR(point.y, 22.0F, 1e-5F);

    const Vec2 back = transform.getInverse().apply(point);
    EXPECT_NEAR(back.x, 1.0F, 1e-5F);
    EXPECT_NEAR(back.y, 0.0F, 1e-5F);

    const Transform2D combined = Transform2D::translation(Vec2(5.0F, 0.0F)) * Transform2D::scaling(Vec2(3.0F, 3.0F));
    EXPECT_EQ(combined.apply(Vec2(1.0F, 1.0F)), Vec2(8.0F, 3.0F));
    EXPECT_EQ(combined.applyVector(Vec2(1.0F, 1.0F)), Vec2(3.0F, 3.0F));
    EXPECT_EQ(combined.getTranslation(), Vec2(5.0F, 0.0F));
    EXPECT_FLOAT_EQ(combined.getDeterminant(), 9.0F);

    const Vec2 rotated = Transform2D::rotation(Math::kPi).apply(Vec2(1.0F, 0.0F));
    EXPECT_NEAR(rotated.x, -1.0F, 1e-6F);
    EXPECT_EQ(Transform2D::identity().apply(Vec2(4.0F, 5.0F)), Vec2(4.0F, 5.0F));
}

TEST(Transform2DTest, SingularInverseCollapsesToZero) {
    const Transform2D singular = Transform2D::scaling(Vec2(0.0F, 1.0F));
    EXPECT_EQ(singular.getInverse().apply(Vec2(3.0F, 4.0F)), Vec2{});
}

TEST(Transform2DTest, SkewShearsTheVerticalAxis) {
    const Transform2D skewed = Transform2D::compose(Vec2{}, 0.0F, Vec2(1.0F, 1.0F), Vec2(Math::kPi / 4.0F, 0.0F));
    const Vec2 top = skewed.apply(Vec2(0.0F, 1.0F));
    EXPECT_NEAR(top.x, -std::sin(Math::kPi / 4.0F), 1e-5F);
}

} // namespace haylen::math
