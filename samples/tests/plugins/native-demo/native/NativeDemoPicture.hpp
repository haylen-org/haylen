#pragma once

#include <array>
#include <cstdint>
#include <span>

// The pictures of the demo. The island is a small island with a palm and its coconuts under the sun and two clouds, in a sea with waves, in fractions of the width and the height of the picture, the same on every platform. Every shape has the outline of the art, the sun a soft halo, the sand and the leaves a lighter highlight and the island a shadow on the water. The video moves the waves and floats the sun with the time.
class NativeDemoPicture final {
  public:
    // The frames per second of the video, which sets the time of each frame.
    static constexpr int kFramesPerSecond = 30;

    // Draws the island at the time of the frame into pixels of four bytes, RGBA or BGRA, with four samples a pixel for smooth edges.
    static void drawIsland(std::span<std::uint8_t> pixels, int width, int height, int frame, bool bgra);

    // Fills a frame of the camera that the demo simulates with RGBA colors that move with the index of the frame, so the app sees each new frame arrive.
    static void fillCameraFrame(std::span<std::uint8_t> pixels, int width, int height, unsigned index);

  private:
    struct Color {
        float red;
        float green;
        float blue;
    };

    // A circle of a cloud, with the x and y of its center in fractions of the width and the height and its radius in fractions of the height.
    struct Circle {
        float x;
        float y;
        float radius;
    };

    static constexpr Color kInk{30, 27, 51};
    static constexpr Color kSkyTop{108, 198, 245};
    static constexpr Color kSkyLow{207, 241, 255};
    static constexpr Color kSeaTop{47, 179, 196};
    static constexpr Color kSeaLow{30, 143, 166};
    static constexpr Color kSun{255, 201, 60};
    static constexpr Color kSunLight{255, 232, 154};
    static constexpr Color kSand{243, 213, 141};
    static constexpr Color kSandLight{251, 232, 182};
    static constexpr Color kGrass{123, 199, 77};
    static constexpr Color kTrunk{168, 106, 61};
    static constexpr Color kLeaf{93, 179, 60};
    static constexpr Color kLeafLight{143, 212, 94};
    static constexpr Color kCoconut{107, 68, 35};
    static constexpr Color kCloud{255, 255, 255};
    static constexpr Color kFoam{255, 255, 255};
    static constexpr Color kHalo{255, 250, 225};
    static constexpr std::array<float, 4> kLeafAngles{-0.45F, 0.4F, 2.75F, 3.55F};
    static constexpr std::array<std::array<Circle, 3>, 2> kClouds{{
        {{{0.14F, 0.22F, 0.06F}, {0.19F, 0.17F, 0.08F}, {0.245F, 0.22F, 0.06F}}},
        {{{0.56F, 0.15F, 0.05F}, {0.6F, 0.12F, 0.065F}, {0.64F, 0.15F, 0.05F}}},
    }};
    static constexpr std::array<std::array<float, 2>, 2> kCoconuts{{{0.41F, 0.33F}, {0.435F, 0.335F}}};

    [[nodiscard]] static Color mix(Color from, Color to, float amount);

    // Whether the point lies inside the ellipse of the center and the radii, turned by the angle in radians.
    [[nodiscard]] static bool inEllipse(float x, float y, float centerX, float centerY, float radiusX, float radiusY, float angle);

    // Whether the point lies within the radius of the segment, a line with round ends.
    [[nodiscard]] static bool inCapsule(float x, float y, float fromX, float fromY, float toX, float toY, float radius);

    // Paints an ellipse with the outline of the art around it, which takes the point when it lies on either.
    static void paintOutlined(Color& color, float x, float y, float centerX, float centerY, float radiusX, float radiusY, float angle, float outline, Color fill);

    // Paints a cloud, whose circles share one outline around the whole cloud.
    static void paintCloud(Color& color, float x, float y, const std::array<Circle, 3>& circles, float width, float height, float outline);

    // The color of one point of the picture at the time in seconds, painted from the back to the front.
    [[nodiscard]] static Color scene(float x, float y, float width, float height, float seconds);
};
