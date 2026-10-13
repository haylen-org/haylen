#include <gtest/gtest.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/animation/AtlasBuilder.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::animation2d {

namespace {

class AtlasBuilderTest : public ::testing::Test {
  protected:
    // An image of the size with a square of the color at the area, transparent around it.
    [[nodiscard]] static std::shared_ptr<graphics::Image> sprite(int width, int height, const graphics::Image::Area& area, math::Color color) {
        auto image = std::make_shared<graphics::Image>(width, height);
        image->fill(area, color);
        return image;
    }

    [[nodiscard]] static bool overlaps(const math::Rect& lhs, const math::Rect& rhs) {
        return lhs.x < rhs.getRight() && rhs.x < lhs.getRight() && lhs.y < rhs.getBottom() && rhs.y < lhs.getBottom();
    }

    test::EngineFixture fixture;
};

} // namespace

TEST_F(AtlasBuilderTest, PacksTrimmedFramesWithoutOverlapAndKeepsTheirPlace) {
    const math::Color red = math::Color::fromHex(0xFF0000FFU);
    AtlasBuilder builder;
    builder.add("ring", sprite(40, 30, {10, 5, 20, 20}, red));
    builder.add("bar", sprite(100, 10, {0, 0, 100, 10}, red));
    for (int index = 0; index < 20; ++index) {
        builder.add("dot" + std::to_string(index), sprite(16, 16, {index % 8, 4, 8, 8}, math::Color::white()));
    }
    const auto shared = sprite(16, 16, {0, 0, 16, 16}, red);
    builder.add("first", shared);
    builder.add("second", shared);
    EXPECT_EQ(builder.size(), 24U);

    const AtlasBuilder::Packed packed = builder.pack({.padding = 4, .extrude = 2});
    std::map<std::string, SpriteFrame> frames(packed.frames.begin(), packed.frames.end());
    const SpriteFrame& ring = frames.at("ring");
    EXPECT_EQ(ring.source.getSize(), math::Vec2(20.0F, 20.0F));
    EXPECT_EQ(ring.offset, math::Vec2(10.0F, 5.0F));
    EXPECT_EQ(ring.originalSize, math::Vec2(40.0F, 30.0F));
    EXPECT_EQ(packed.image.getPixel(static_cast<int>(ring.source.x), static_cast<int>(ring.source.y)), red);

    // Identical pixels share one place, every other frame has its own with the padding around it, and the extrusion repeats the edges into it.
    EXPECT_EQ(frames.at("first").source, frames.at("second").source);
    for (const auto& [name, frame] : packed.frames) {
        EXPECT_GE(frame.source.x, 2.0F) << name;
        EXPECT_LE(frame.source.getRight(), static_cast<float>(packed.image.getWidth()) - 2.0F) << name;
        for (const auto& [other, placed] : packed.frames) {
            if (other != name && !(name == "first" && other == "second") && !(name == "second" && other == "first")) {
                EXPECT_FALSE(overlaps(frame.source.expanded(1.0F), placed.source)) << name << " " << other;
            }
        }
    }
    EXPECT_EQ(packed.image.getPixel(static_cast<int>(ring.source.x) - 2, static_cast<int>(ring.source.y) - 2), red);
    EXPECT_LE(packed.image.getWidth(), 256);

    const SpriteAtlas atlas = AtlasBuilder::finish(packed, fixture.engine().getGraphics().createTexture(packed.image));
    EXPECT_EQ(atlas.getFrame("ring").source, ring.source);
    EXPECT_EQ(atlas.getFrameNames().size(), 24U);

    // Without trimming every frame keeps its whole image.
    const AtlasBuilder::Packed whole = builder.pack({.padding = 0, .extrude = 0, .trim = false});
    EXPECT_EQ(whole.frames.front().second.source.getSize(), math::Vec2(40.0F, 30.0F));
    EXPECT_EQ(whole.frames.front().second.offset, math::Vec2(0.0F, 0.0F));
}

TEST_F(AtlasBuilderTest, CarriesTheFramesAndAnimationsOfAtlases) {
    auto sheet = std::make_shared<graphics::Image>(64, 32);
    sheet->fill({0, 0, 32, 32}, math::Color::fromHex(0xFF0000FFU));
    sheet->fill({40, 8, 16, 16}, math::Color::fromHex(0x00FF00FFU));
    const graphics::Texture texture = fixture.engine().getGraphics().createTexture(*sheet);
    // clang-format off
    const SpriteAtlas source = SpriteAtlas::parse(nlohmann::ordered_json::parse(R"json({
        "frames": [
            {"filename": "run 0", "frame": {"x": 0, "y": 0, "w": 32, "h": 32}, "duration": 100},
            {"filename": "run 1", "frame": {"x": 32, "y": 0, "w": 32, "h": 32}, "duration": 250}
        ],
        "meta": {"image": "sheet.png", "frameTags": [{"name": "run", "from": 0, "to": 1, "direction": "forward"}]}
    })json"), texture);
    // clang-format on

    AtlasBuilder builder;
    builder.addAtlas("hero/", source, sheet);
    builder.add("extra", sprite(8, 8, {0, 0, 8, 8}, math::Color::white()));
    const AtlasBuilder::Packed packed = builder.pack({});
    ASSERT_EQ(packed.animations.size(), 1U);
    EXPECT_EQ(packed.animations[0].first, "hero/run");
    const Animation& run = packed.animations[0].second;
    std::map<std::string, SpriteFrame> frames(packed.frames.begin(), packed.frames.end());
    EXPECT_EQ(run.frames[0].source, frames.at("hero/run 0").source);
    EXPECT_EQ(run.frames[1].source, frames.at("hero/run 1").source);
    EXPECT_EQ(run.frames[1].offset, math::Vec2(8.0F, 8.0F));
    EXPECT_FLOAT_EQ(run.frames[1].duration, 0.25F);

    const SpriteAtlas atlas = AtlasBuilder::finish(packed, fixture.engine().getGraphics().createTexture(packed.image));
    EXPECT_EQ(atlas.getAnimation("hero/run").texture, atlas.getTexture());
    EXPECT_TRUE(atlas.hasFrame("extra"));

    // A subset names the frames and animations of a prefix without it, over the same texture.
    const SpriteAtlas hero = atlas.subset("hero/");
    EXPECT_EQ(hero.getFrameNames(), (std::vector<std::string>{"run 0", "run 1"}));
    EXPECT_EQ(hero.getAnimation("run").frames.size(), 2U);
    EXPECT_EQ(hero.getTexture(), atlas.getTexture());
    EXPECT_FALSE(hero.hasFrame("extra"));
}

TEST_F(AtlasBuilderTest, RejectsWhatCannotPack) {
    AtlasBuilder builder;
    EXPECT_THROW((void)builder.pack({}), std::invalid_argument);
    builder.add("big", sprite(300, 20, {0, 0, 300, 20}, math::Color::white()));
    EXPECT_THROW(builder.add("big", sprite(2, 2, {0, 0, 2, 2}, math::Color::white())), std::invalid_argument);
    EXPECT_THROW(builder.add("outside", sprite(4, 4, {0, 0, 4, 4}, math::Color::white()), graphics::Image::Area{2, 2, 4, 4}), std::invalid_argument);
    EXPECT_THROW(builder.add("empty", nullptr), std::invalid_argument);
    EXPECT_THROW((void)builder.pack({.padding = 2, .extrude = 2}), std::invalid_argument);
    EXPECT_THROW((void)builder.pack({.maxSize = 256}), std::runtime_error);
    EXPECT_EQ(builder.pack({.maxSize = 512}).image.getWidth(), 512);
}

} // namespace haylen::animation2d
