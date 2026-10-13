#include <gtest/gtest.h>

#include <array>
#include <string>

#include "haylen/2d/graphics/PostProcess.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::graphics2d {

namespace {

std::array<float, 3> apply(const std::array<std::array<float, 3>, 3>& matrix, std::array<float, 3> color) {
    std::array<float, 3> result{};
    for (std::size_t row = 0; row < 3; ++row) {
        result[row] = matrix[row][0] * color[0] + matrix[row][1] * color[1] + matrix[row][2] * color[2];
    }
    return result;
}

} // namespace

TEST(PostProcessTest, SimulatesAndCorrectsDeficienciesOfColorVision) {
    PostProcess post;
    EXPECT_EQ(apply(post.getColorVisionMatrix(), {0.25F, 0.5F, 0.75F}), (std::array<float, 3>{0.25F, 0.5F, 0.75F}));

    // Without green cones, red and green look alike, and a gray stays gray.
    post.colorVision = PostProcess::ColorVision::Deuteranopia;
    const std::array<float, 3> red = apply(post.getColorVisionMatrix(), {1.0F, 0.0F, 0.0F});
    const std::array<float, 3> green = apply(post.getColorVisionMatrix(), {0.0F, 1.0F, 0.0F});
    EXPECT_NEAR(red[0] / red[1], green[0] / green[1], 0.15F);
    const std::array<float, 3> gray = apply(post.getColorVisionMatrix(), {0.5F, 0.5F, 0.5F});
    for (const float channel : gray) {
        EXPECT_NEAR(channel, 0.5F, 0.001F);
    }

    // The correction moves what the simulation loses into blue, so red and green differ in blue for the same player.
    post.correctColorVision = true;
    const std::array<float, 3> correctedRed = apply(post.getColorVisionMatrix(), {1.0F, 0.0F, 0.0F});
    const std::array<float, 3> correctedGreen = apply(post.getColorVisionMatrix(), {0.0F, 1.0F, 0.0F});
    EXPECT_GT(correctedRed[2] - correctedGreen[2], 0.5F);
    for (const PostProcess::ColorVision vision : {PostProcess::ColorVision::Protanopia, PostProcess::ColorVision::Tritanopia}) {
        post.colorVision = vision;
        for (const float channel : apply(post.getColorVisionMatrix(), {0.5F, 0.5F, 0.5F})) {
            EXPECT_NEAR(channel, 0.5F, 0.001F);
        }
    }
}

TEST(PostProcessTest, ReadsTheColorVisionOptionsFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("graphics2d = require('haylen.graphics2d') camera = graphics2d.newCamera()");
    for (const char* vision : {"normal", "protanopia", "deuteranopia", "tritanopia"}) {
        EXPECT_EQ(fixture.lua(std::string("graphics2d.beginWorld(camera, {postProcess = {colorVision = '") + vision + "', correctColorVision = true}}) graphics2d.drawRect({0, 0, 10, 10}, '#FF0000') return 'ok'"), "ok");
    }
    fixture.frames(1);
    EXPECT_NE(fixture.lua("graphics2d.beginWorld(camera, {postProcess = {colorVision = 'gray'}})").find("The color vision \"gray\" is unknown. It is \"normal\", \"protanopia\", \"deuteranopia\" or \"tritanopia\"."), std::string::npos);
}

} // namespace haylen::graphics2d
