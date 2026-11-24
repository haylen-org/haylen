#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/Animation.hpp"
#include "haylen/2d/animation/Animator.hpp"
#include "haylen/2d/animation/SpriteAtlas.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Insets.hpp"
#include "support/EngineFixture.hpp"
#include "support/SpriteAtlasFiles.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

TEST(SpriteAtlasTest, ReadsArrayFramesTagsAndSlices) {
    test::EngineFixture fixture;
    const haylen::graphics::Texture texture = fixture.engine().getGraphics().createTexture(haylen::graphics::Image(64, 32));
    const auto document = nlohmann::ordered_json::parse(test::SpriteAtlasFiles::kTaggedAtlas);
    const SpriteAtlas atlas = SpriteAtlas::parse(document, texture);

    EXPECT_EQ(SpriteAtlas::imagePath(document), "hero.png");
    EXPECT_EQ(atlas.getFrameNames(), (std::vector<std::string>{"hero 0", "hero 1", "hero 2", "panel"}));
    EXPECT_EQ(atlas.getFrame("hero 0").offset, haylen::math::Vec2(4.0F, 0.0F));
    EXPECT_EQ(atlas.getFrame("hero 0").originalSize, haylen::math::Vec2(16.0F, 16.0F));
    EXPECT_FLOAT_EQ(atlas.getFrame("hero 1").duration, 0.2F);
    EXPECT_TRUE(atlas.hasFrame("panel"));

    EXPECT_EQ(atlas.getAnimationNames(), (std::vector<std::string>{"back", "bounce", "hit", "walk"}));
    EXPECT_FLOAT_EQ(atlas.getAnimation("walk").getDuration(), 0.4F);
    EXPECT_EQ(atlas.getAnimation("back").frames.front().source.x, 24.0F);
    EXPECT_EQ(atlas.getAnimation("bounce").loop, Animation::Loop::PingPong);
    EXPECT_EQ(atlas.getAnimation("hit").loop, Animation::Loop::Once);
    EXPECT_TRUE(atlas.hasAnimation("walk"));

    EXPECT_EQ(atlas.getSliceNames(), (std::vector<std::string>{"panel"}));
    EXPECT_EQ(atlas.getSlice("panel").getBorders(), (haylen::math::Insets{.left = 4.0F, .top = 4.0F, .right = 4.0F, .bottom = 4.0F}));
    EXPECT_TRUE(atlas.hasSlice("panel"));
    EXPECT_FALSE(atlas.hasSlice("marker"));

    const Animation custom = atlas.animationFromFrames({"hero 2", "hero 0"}, 4.0F, Animation::Loop::Once);
    EXPECT_FLOAT_EQ(custom.getDuration(), 0.5F);

    Animator animator;
    animator.add("walk", atlas.getAnimation("walk"));
    animator.play("walk");
    haylen::graphics2d::Sprite sprite;
    animator.apply(sprite);
    EXPECT_EQ(sprite.pivot, haylen::math::Vec2(0.5F, 0.5F));
    EXPECT_EQ(sprite.size, haylen::math::Vec2(8.0F, 16.0F));

    EXPECT_THROW((void)atlas.getFrame("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.getAnimation("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.getSlice("missing"), std::invalid_argument);
    EXPECT_THROW((void)atlas.animationFromFrames({}, 4.0F), std::invalid_argument);
    EXPECT_THROW((void)SpriteAtlas::parse(document, haylen::graphics::Texture{}), std::invalid_argument);
}

// Slice bounds are relative to the untrimmed canvas of their frame, so a trimmed frame moves its slice by the pixels it dropped.
TEST(SpriteAtlasTest, PlacesTheSlicesOfTrimmedFramesInTheSheet) {
    test::EngineFixture fixture;
    const haylen::graphics::Texture texture = fixture.engine().getGraphics().createTexture(haylen::graphics::Image(64, 32));
    // clang-format off
    const auto trimmed = [](const char* bounds) {
        return nlohmann::ordered_json::parse(std::string(R"({
            "frames": [{"filename": "panel", "frame": {"x": 40, "y": 10, "w": 20, "h": 12}, "trimmed": true, "spriteSourceSize": {"x": 2, "y": 3, "w": 20, "h": 12}, "sourceSize": {"w": 24, "h": 18}}],
            "meta": {"image": "ui.png", "slices": [{"name": "panel", "keys": [{"frame": 0, "bounds": )") + bounds + R"(, "center": {"x": 4, "y": 4, "w": 10, "h": 4}}]}]}
        })");
    };
    // clang-format on

    const SpriteAtlas atlas = SpriteAtlas::parse(trimmed(R"({"x": 4, "y": 3, "w": 18, "h": 12})"), texture);
    const haylen::graphics2d::NineSlice& slice = atlas.getSlice("panel");
    EXPECT_EQ(slice.pieces[0], (haylen::math::Rect{42.0F, 10.0F, 4.0F, 4.0F}));
    EXPECT_EQ(slice.pieces[8], (haylen::math::Rect{56.0F, 18.0F, 4.0F, 4.0F}));
    EXPECT_THROW((void)SpriteAtlas::parse(trimmed(R"({"x": 0, "y": 0, "w": 24, "h": 18})"), texture), std::invalid_argument);
}

TEST(SpriteAtlasTest, ReadsHashFramesAndRejectsUnsupportedData) {
    test::EngineFixture fixture;
    const haylen::graphics::Texture texture = fixture.engine().getGraphics().createTexture(haylen::graphics::Image(32, 32));

    const SpriteAtlas hash = SpriteAtlas::parse(nlohmann::ordered_json::parse(R"({"frames": {"a.png": {"frame": {"x": 0, "y": 0, "w": 8, "h": 8}}, "b.png": {"frame": {"x": 8, "y": 0, "w": 8, "h": 8}}}, "meta": {"image": "sheet.png"}})"), texture);
    EXPECT_EQ(hash.getFrameNames().size(), 2U);
    EXPECT_EQ(hash.getFrame("b.png").source.x, 8.0F);
    EXPECT_TRUE(hash.getAnimationNames().empty());

    const auto parse = [&texture](const char* text) { return SpriteAtlas::parse(nlohmann::ordered_json::parse(text), texture); };
    EXPECT_THROW((void)parse(R"({"frames": {"a": {"frame": {"x": 0, "y": 0, "w": 8, "h": 8}, "rotated": true}}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 3}]}})"), std::out_of_range);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 0, "direction": "sideways"}]}})"), std::invalid_argument);
    EXPECT_THROW((void)parse(R"({"frames": [{"filename": "a", "frame": {"x": 0, "y": 0, "w": 8, "h": 8}}], "meta": {"frameTags": [{"name": "t", "from": 0, "to": 0, "repeat": "3"}]}})"), std::invalid_argument);
}

TEST(SpriteAtlasTest, KeepsHashFramesInFileOrder) {
    // Frames named `hero 0` to `hero 11` keep the order of the file, where `hero 10` comes after `hero 9`, because frame tags count positions in it.
    std::string frames;
    for (int index = 0; index < 12; ++index) {
        frames += (index == 0 ? "" : ", ") + std::string(R"("hero )") + std::to_string(index) + R"(": {"frame": {"x": )" + std::to_string(index * 8) + R"(, "y": 0, "w": 8, "h": 8}})";
    }

    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(96, 8, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/hero.json", R"({"frames": {)" + frames + R"(}, "meta": {"image": "hero.png", "frameTags": [{"name": "walk", "from": 0, "to": 11}]}})"}, {"content/hero.png", std::string(image.begin(), image.end())}});

    const auto atlas = std::static_pointer_cast<SpriteAtlas>(fixture.engine().getAssets().load("atlas", "hero.json"));
    EXPECT_EQ(atlas->getFrameNames()[10], "hero 10");
    const Animation& walk = atlas->getAnimation("walk");
    ASSERT_EQ(walk.frames.size(), 12U);

    for (std::size_t index = 0; index < walk.frames.size(); ++index) {
        EXPECT_EQ(walk.frames[index].source.x, static_cast<float>(index * 8)) << index;
    }
}

TEST(SpriteAtlasTest, LoadsThroughAssetsAndSharesItsTexture) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(64, 32, 0xFFFFFFFFU);
    test::EngineFixture fixture({{"content/ui/hero.json", test::SpriteAtlasFiles::kTaggedAtlas}, {"content/ui/hero.png", std::string(image.begin(), image.end())}});
    haylen::assets::Manager& assets = fixture.engine().getAssets();

    const auto atlas = std::static_pointer_cast<SpriteAtlas>(assets.load("atlas", "ui/hero.json"));
    EXPECT_EQ(atlas->getTexture(), assets.texture("ui/hero.png"));
    EXPECT_EQ(std::static_pointer_cast<SpriteAtlas>(assets.load("atlas", "ui/hero.json", {{"filter", "nearest"}})), atlas);

    std::shared_ptr<SpriteAtlas> smooth;
    // clang-format off
    assets.loadAsync("atlas", "ui/hero.json", [&](std::shared_ptr<void> asset, std::string) {
        smooth = std::static_pointer_cast<SpriteAtlas>(asset);
    }, {{"filter", "linear"}});
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return smooth != nullptr; }));
    EXPECT_NE(smooth, atlas);
    EXPECT_EQ(smooth->getTexture(), assets.texture("ui/hero.png", {.filter = haylen::graphics::Texture::Filter::Linear}));

    std::string error;
    assets.loadAsync("atlas", "ui/missing.json", [&](std::shared_ptr<void>, std::string message) { error = message; });
    ASSERT_TRUE(fixture.frameUntil([&] { return !error.empty(); }));
    EXPECT_THROW((void)assets.load("atlas", "ui/hero.json", {{"scale", 2}}), std::invalid_argument);
}

} // namespace haylen::animation2d
