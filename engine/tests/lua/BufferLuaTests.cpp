#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/graphics/Device.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::core {

TEST(FloatBufferTest, ChecksBoundsAndSetsInBulk) {
    FloatBuffer buffer(4, 1.5F);
    EXPECT_EQ(buffer.size(), 4U);
    EXPECT_EQ(buffer.get(3), 1.5F);
    buffer.set(1, 7.0F);
    const std::vector<float> values{2.0F, 3.0F};
    buffer.set(2, values);
    EXPECT_EQ(std::vector<float>(buffer.getValues().begin(), buffer.getValues().end()), (std::vector<float>{1.5F, 7.0F, 2.0F, 3.0F}));
    EXPECT_THROW((void)buffer.get(4), std::out_of_range);
    EXPECT_THROW(buffer.set(4, 0.0F), std::out_of_range);
    EXPECT_THROW(buffer.set(3, values), std::out_of_range);
    EXPECT_THROW(buffer.set(5, std::span<const float>{}), std::out_of_range);
    buffer.fill(0.25F);
    EXPECT_EQ(buffer.get(0), 0.25F);
}

TEST(FloatBufferLuaTest, ReadsAndWritesFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("buffer = require('haylen.collections').newFloatBuffer(6)");
    EXPECT_EQ(fixture.lua("buffer[1] = 2.5 buffer[6] = -1 return #buffer .. ' ' .. buffer[1] .. ' ' .. buffer[2] .. ' ' .. buffer[6]"), "6 2.5 0.0 -1.0");
    EXPECT_EQ(fixture.lua("buffer:set(2, 10, 20, 30) return table.concat({buffer:get(1, 5)}, ',')"), "2.5,10.0,20.0,30.0,0.0");
    EXPECT_EQ(fixture.lua("buffer:set(4, {7, 8, 9}) return table.concat({buffer:get(4, 3)}, ',') .. ' ' .. buffer:get(1)"), "7.0,8.0,9.0 2.5");
    EXPECT_EQ(fixture.lua("buffer:fill(1) buffer:fill(4, 2, 2) return table.concat({buffer:get(1, 6)}, ',')"), "1.0,4.0,4.0,1.0,1.0,1.0");
    EXPECT_EQ(fixture.lua("return require('haylen.collections').newFloatBuffer(2, 3)[2]"), "3.0");

    EXPECT_NE(fixture.lua("return buffer[0]").find("positions 0 to 0 fall outside its size of 6"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer[7] = 1").find("positions 7 to 7 fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer[1.5]").find("fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:set(5, 1, 2, 3)").find("positions 5 to 7 fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:set(1, {1, 'two'})").find("entry 2 of the list is not"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer[1] = 'x'").find("number expected"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:get(1, -1)").find("the count cannot be negative"), std::string::npos);
    EXPECT_NE(fixture.lua("buffer:fill(0, 6, 2)").find("fall outside"), std::string::npos);
    EXPECT_NE(fixture.lua("require('haylen.collections').newFloatBuffer(-1)").find("the size cannot be negative"), std::string::npos);
    EXPECT_NE(fixture.lua("return buffer.missing").find("has no member 'missing'"), std::string::npos);
}

} // namespace haylen::core

namespace haylen::graphics2d {

TEST(SpriteLayoutTest, MovesSpriteFieldsThroughFloats) {
    const SpriteLayout layout({SpriteLayout::Field::X, SpriteLayout::Field::Y, SpriteLayout::Field::Rotation, SpriteLayout::Field::Alpha}, {.size = {4.0F, 4.0F}});
    EXPECT_EQ(layout.getStride(), 4U);
    const std::vector<float> values{1.0F, 2.0F, 0.5F, 0.25F, 3.0F, 4.0F, 1.0F, 1.0F, 9.0F};
    EXPECT_EQ(layout.getCount(values), 2U);
    const SpriteInstance second = layout.makeSprite(values, 1);
    EXPECT_EQ(second.position, math::Vec2(3.0F, 4.0F));
    EXPECT_EQ(second.rotation, 1.0F);
    EXPECT_EQ(second.size, math::Vec2(4.0F, 4.0F));

    std::vector<float> stored(4);
    layout.store(layout.makeSprite(values, 0), stored, 0);
    EXPECT_EQ(stored, (std::vector<float>{1.0F, 2.0F, 0.5F, 0.25F}));
    EXPECT_THROW(SpriteLayout({}), std::invalid_argument);
    EXPECT_EQ(SpriteLayout::fieldFromName("sourceWidth"), SpriteLayout::Field::SourceWidth);
    EXPECT_EQ(SpriteLayout::fieldName(SpriteLayout::Field::PivotY), "pivotY");
    EXPECT_FALSE(SpriteLayout::fieldFromName("depth").has_value());
}

TEST(SpriteLayoutTest, UpdatesBatchesInBulk) {
    test::EngineFixture fixture;
    SpriteBatch batch(fixture.engine().getGraphics().getWhiteTexture());
    batch.resize(3, {.size = {2.0F, 2.0F}});
    const SpriteLayout layout({SpriteLayout::Field::X, SpriteLayout::Field::Y});
    const std::vector<float> values{1.0F, 2.0F, 3.0F, 4.0F};
    batch.writeFields(values, layout, 1);
    EXPECT_EQ(batch.get(1).position, math::Vec2(1.0F, 2.0F));
    EXPECT_EQ(batch.get(2).position, math::Vec2(3.0F, 4.0F));
    EXPECT_EQ(batch.get(2).size, math::Vec2(2.0F, 2.0F));

    std::vector<float> read(6);
    batch.readFields(read, layout);
    EXPECT_EQ(read, (std::vector<float>{0.0F, 0.0F, 1.0F, 2.0F, 3.0F, 4.0F}));
    EXPECT_THROW(batch.writeFields(values, layout, 4), std::out_of_range);
    EXPECT_THROW(batch.readFields(read, layout, 4), std::out_of_range);
}

TEST(SpriteLayoutTest, DrawsAndUpdatesSpritesFromBuffersInLua) {
    test::EngineFixture fixture;
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
    EXPECT_NE(fixture.lua("batch:writeFields(buffer, {'x'}, 0)").find("numbered from one"), std::string::npos);
    EXPECT_NE(fixture.lua("batch:resize(-1)").find("the count cannot be negative"), std::string::npos);
}

TEST(SpriteLayoutTest, CopiesBodyTransformsAndParticlePositionsInBulk) {
    test::EngineFixture fixture;
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

} // namespace haylen::graphics2d
