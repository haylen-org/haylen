#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::graphics2d {

TEST(SpriteLayoutTest, MovesSpriteFieldsThroughFloats) {
    const haylen::graphics2d::SpriteLayout layout(std::array{haylen::graphics2d::SpriteLayout::Field::X, haylen::graphics2d::SpriteLayout::Field::Y, haylen::graphics2d::SpriteLayout::Field::Rotation, haylen::graphics2d::SpriteLayout::Field::Alpha}, {.size = {4.0F, 4.0F}});
    EXPECT_EQ(layout.getStride(), 4U);
    const std::vector<float> values{1.0F, 2.0F, 0.5F, 0.25F, 3.0F, 4.0F, 1.0F, 1.0F, 9.0F};
    EXPECT_EQ(layout.getCount(values), 2U);
    const haylen::graphics2d::SpriteInstance second = layout.makeSprite(values, 1);
    EXPECT_EQ(second.position, haylen::math::Vec2(3.0F, 4.0F));
    EXPECT_EQ(second.rotation, 1.0F);
    EXPECT_EQ(second.size, haylen::math::Vec2(4.0F, 4.0F));

    std::vector<float> stored(4);
    layout.store(layout.makeSprite(values, 0), {}, stored, 0);
    EXPECT_EQ(stored, (std::vector<float>{1.0F, 2.0F, 0.5F, 0.25F}));
    EXPECT_THROW(haylen::graphics2d::SpriteLayout({}), std::invalid_argument);
    EXPECT_THROW(haylen::graphics2d::SpriteLayout(std::array{haylen::graphics2d::SpriteLayout::Field::X, haylen::graphics2d::SpriteLayout::Field::Y, haylen::graphics2d::SpriteLayout::Field::X}), std::invalid_argument);
    EXPECT_EQ(haylen::graphics2d::SpriteLayout::fieldFromName("sourceWidth"), haylen::graphics2d::SpriteLayout::Field::SourceWidth);
    EXPECT_EQ(haylen::graphics2d::SpriteLayout::fieldName(haylen::graphics2d::SpriteLayout::Field::PivotY), "pivotY");
    EXPECT_FALSE(haylen::graphics2d::SpriteLayout::fieldFromName("depth").has_value());
}

// Every field a layout names reads and writes the value of the sprite or of its data that it stands for, where the texture counts from 1.
TEST(SpriteLayoutTest, StoresAndAppliesEveryField) {
    std::vector<haylen::graphics2d::SpriteLayout::Field> fields;
    std::vector<float> values;
    for (int field = 0; field <= static_cast<int>(haylen::graphics2d::SpriteLayout::Field::Data4); ++field) {
        fields.push_back(static_cast<haylen::graphics2d::SpriteLayout::Field>(field));
        values.push_back(static_cast<float>(field + 1) * 0.5F);
    }

    values[static_cast<std::size_t>(haylen::graphics2d::SpriteLayout::Field::Texture)] = 9.0F;
    const haylen::graphics2d::SpriteLayout layout(fields);
    const haylen::graphics2d::SpriteInstance sprite = layout.makeSprite(values, 0);
    EXPECT_EQ(sprite.position, haylen::math::Vec2(0.5F, 1.0F));
    EXPECT_EQ(sprite.size, haylen::math::Vec2(1.5F, 2.0F));
    EXPECT_EQ(sprite.rotation, 2.5F);
    EXPECT_EQ(sprite.tilt, 3.0F);
    EXPECT_EQ(sprite.pivot, haylen::math::Vec2(3.5F, 4.0F));
    EXPECT_EQ(sprite.color, (haylen::math::Color{4.5F, 5.0F, 5.5F, 6.0F}));
    EXPECT_EQ(sprite.source, (haylen::math::Rect{6.5F, 7.0F, 7.5F, 8.0F}));
    const haylen::graphics2d::SpriteData data = layout.makeData(values, 0);
    EXPECT_EQ(data, (haylen::graphics2d::SpriteData{.texture = 8, .values = {9.0F, 9.5F, 10.0F, 10.5F}}));
    EXPECT_TRUE(layout.hasData());
    EXPECT_FALSE(haylen::graphics2d::SpriteLayout(std::array{haylen::graphics2d::SpriteLayout::Field::X}).hasData());
    EXPECT_TRUE(haylen::graphics2d::SpriteLayout(std::array{haylen::graphics2d::SpriteLayout::Field::X}, {}, {.texture = 1}).hasData());

    std::vector<float> stored(values.size());
    layout.store(sprite, data, stored, 0);
    EXPECT_EQ(stored, values);
}

TEST(SpriteLayoutTest, UpdatesBatchesInBulk) {
    test::LuaFixture fixture;
    haylen::graphics2d::SpriteBatch batch(fixture.engine().getGraphics().getWhiteTexture());
    batch.resize(3, {.size = {2.0F, 2.0F}});
    const haylen::graphics2d::SpriteLayout layout(std::array{haylen::graphics2d::SpriteLayout::Field::X, haylen::graphics2d::SpriteLayout::Field::Y});
    const std::vector<float> values{1.0F, 2.0F, 3.0F, 4.0F};
    batch.writeFields(values, layout, 1);
    EXPECT_EQ(batch.get(1).position, haylen::math::Vec2(1.0F, 2.0F));
    EXPECT_EQ(batch.get(2).position, haylen::math::Vec2(3.0F, 4.0F));
    EXPECT_EQ(batch.get(2).size, haylen::math::Vec2(2.0F, 2.0F));

    std::vector<float> read(6);
    batch.readFields(read, layout);
    EXPECT_EQ(read, (std::vector<float>{0.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F}));
    EXPECT_THROW(batch.writeFields(values, layout, 4), std::out_of_range);
    EXPECT_THROW(batch.readFields(read, layout, 4), std::out_of_range);
}

TEST(SpriteLayoutTest, DrawsAndUpdatesSpritesFromBuffersInLua) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        buffer = require('haylen.collections').newFloatBuffer(8)
        for index = 1, 8 do buffer[index] = index * 10 end
        batch = graphics2d.newSpriteBatch(graphics.whiteTexture())
        batch:resize(4, {width = 3, height = 5})
        batch:writeFields(buffer, {'x', 'y'}, 2)
        local copy = require('haylen.collections').newFloatBuffer(4)
        batch:readFields(copy, {'width', 'rotation'}, 3)
        summary = batch:size() .. ' ' .. batch:get(2).x .. ',' .. batch:get(2).y .. ' ' .. batch:get(4).y .. ' ' .. batch:get(1).x .. ' ' .. copy[1] .. ',' .. copy[2] .. ',' .. copy[3]
        require('haylen.scene').push({render = function()
            graphics2d.beginScreen()
            graphics2d.drawBatch(graphics.whiteTexture(), buffer, {fields = {'x', 'y'}, width = 4, height = 4, color = '#FF8000'})
            batch:draw()
        end})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return summary"), "4 10.0,20.0 60.0 0.0 3.0,0.0,3.0");
    fixture.frames(2);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 8U);

    EXPECT_NE(fixture.lua("graphics2d.beginScreen() graphics2d.drawBatch(graphics.whiteTexture(), buffer, {width = 4})").find("needs the fields each sprite takes"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:writeFields(buffer, {'x', 'depth'})").find("unknown value 'depth'"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:writeFields(buffer, {})").find("at least one field"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:writeFields(buffer, {'x', 'y', 'x'})").find("A sprite layout names each field once, and \"x\" comes twice."), std::string::npos);
    EXPECT_NE(fixture.lua("batch:writeFields(buffer, {'x'}, 0)").find("numbered from one"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:resize(-1)").find("the count cannot be negative"), std::string::npos);
}

TEST(SpriteLayoutTest, CopiesBodyTransformsAndParticlePositionsInBulk) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        physics2d = require('haylen.physics2d')
        buffer = require('haylen.collections').newFloatBuffer(7)
        world = physics2d.newWorld()
        bodies = {world:createBody({x = 10, y = 20, rotation = 0.5}), world:createBody({x = -4, y = 8})}
        world:readTransforms(bodies, buffer, 2)
        read = string.format('%g %g %.2f %g %g', buffer[2], buffer[3], buffer[4], buffer[5], buffer[6])
        buffer:set(1, {100, 200, 0, 300, 400, 1})
        world:writeTransforms(bodies, buffer)
        moved = string.format('%g %g %g %.2f', bodies[1].position.x, bodies[1].position.y, bodies[2].position.x, bodies[2].rotation)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return read"), "10 20 0.50 -4 8");
    EXPECT_EQ(fixture.lua("return moved"), "100 200 300 1.00");
    EXPECT_NE(fixture.lua("world:readTransforms(bodies, buffer, 3)").find("three floats for each body"), std::string::npos);
    EXPECT_NE(fixture.lua("world:readTransforms({bodies[1], physics2d.newWorld():createBody()}, buffer)").find("live bodies of this world"), std::string::npos);
    EXPECT_NE(fixture.lua("world:readTransforms(bodies, buffer, 9)").find("outside the buffer"), std::string::npos);

    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local particles2d = require('haylen.particles2d')
        local emitter = particles2d.newEmitter({texture = require('haylen.graphics').whiteTexture(), rate = 0, speed = {0, 0}})
        emitter.position = require('haylen.math').vec2(5, 6)
        emitter:burst(3)
        local positions = require('haylen.collections').newFloatBuffer(5)
        local count = emitter:readPositions(positions)
        return count .. ' ' .. positions[1] .. ',' .. positions[2] .. ' ' .. positions[5]
    )"), "2 5.0,6.0 0.0");
    // clang-format on
}

} // namespace haylen::lua::graphics2d
