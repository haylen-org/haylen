#include "NativeDemoPicture.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

void NativeDemoPicture::drawIsland(std::span<std::uint8_t> pixels, int width, int height, int frame, bool bgra) {
    const float seconds = static_cast<float>(frame) / static_cast<float>(kFramesPerSecond);
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            Color sum{0, 0, 0};
            for (int sample = 0; sample < 4; ++sample) {
                const float x = static_cast<float>(column) + 0.25F + 0.5F * static_cast<float>(sample % 2);
                const float y = static_cast<float>(row) + 0.25F + 0.5F * static_cast<float>(sample / 2);
                const Color color = scene(x, y, static_cast<float>(width), static_cast<float>(height), seconds);
                sum.red += color.red;
                sum.green += color.green;
                sum.blue += color.blue;
            }

            const std::size_t offset = (static_cast<std::size_t>(row) * static_cast<std::size_t>(width) + static_cast<std::size_t>(column)) * 4;
            pixels[offset + (bgra ? 2 : 0)] = static_cast<std::uint8_t>(sum.red / 4.0F + 0.5F);
            pixels[offset + 1] = static_cast<std::uint8_t>(sum.green / 4.0F + 0.5F);
            pixels[offset + (bgra ? 0 : 2)] = static_cast<std::uint8_t>(sum.blue / 4.0F + 0.5F);
            pixels[offset + 3] = 255;
        }
    }
}

void NativeDemoPicture::fillCameraFrame(std::span<std::uint8_t> pixels, int width, int height, unsigned index) {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
            pixels[offset] = static_cast<std::uint8_t>(static_cast<unsigned>(x * 255 / width) + index * 3);
            pixels[offset + 1] = static_cast<std::uint8_t>(static_cast<unsigned>(y * 255 / height) + index * 2);
            pixels[offset + 2] = static_cast<std::uint8_t>((static_cast<unsigned>(x + y) + index * 4) / 2);
            pixels[offset + 3] = 255;
        }
    }
}

NativeDemoPicture::Color NativeDemoPicture::mix(Color from, Color to, float amount) {
    return {from.red + (to.red - from.red) * amount, from.green + (to.green - from.green) * amount, from.blue + (to.blue - from.blue) * amount};
}

bool NativeDemoPicture::inEllipse(float x, float y, float centerX, float centerY, float radiusX, float radiusY, float angle) {
    const float dx = x - centerX;
    const float dy = y - centerY;
    const float along = dx * std::cos(angle) + dy * std::sin(angle);
    const float across = -dx * std::sin(angle) + dy * std::cos(angle);
    return (along / radiusX) * (along / radiusX) + (across / radiusY) * (across / radiusY) <= 1.0F;
}

bool NativeDemoPicture::inCapsule(float x, float y, float fromX, float fromY, float toX, float toY, float radius) {
    const float segmentX = toX - fromX;
    const float segmentY = toY - fromY;
    const float projected = ((x - fromX) * segmentX + (y - fromY) * segmentY) / (segmentX * segmentX + segmentY * segmentY);
    const float along = std::clamp(projected, 0.0F, 1.0F);
    const float dx = x - (fromX + segmentX * along);
    const float dy = y - (fromY + segmentY * along);
    return dx * dx + dy * dy <= radius * radius;
}

void NativeDemoPicture::paintOutlined(Color& color, float x, float y, float centerX, float centerY, float radiusX, float radiusY, float angle, float outline, Color fill) {
    if (inEllipse(x, y, centerX, centerY, radiusX, radiusY, angle)) {
        color = fill;
        return;
    }
    if (inEllipse(x, y, centerX, centerY, radiusX + outline, radiusY + outline, angle)) {
        color = kInk;
    }
}

void NativeDemoPicture::paintCloud(Color& color, float x, float y, const std::array<Circle, 3>& circles, float width, float height, float outline) {
    bool edge = false;
    for (const Circle& circle : circles) {
        const float centerX = circle.x * width;
        const float centerY = circle.y * height;
        const float radius = circle.radius * height;
        if (inEllipse(x, y, centerX, centerY, radius, radius, 0.0F)) {
            color = kCloud;
            return;
        }
        edge = edge || inEllipse(x, y, centerX, centerY, radius + outline, radius + outline, 0.0F);
    }

    if (edge) {
        color = kInk;
    }
}

NativeDemoPicture::Color NativeDemoPicture::scene(float x, float y, float width, float height, float seconds) {
    const float horizon = 0.6F * height;
    const float outline = 0.012F * height;
    Color color = y < horizon ? mix(kSkyTop, kSkyLow, y / horizon) : mix(kSeaTop, kSeaLow, (y - horizon) / (height - horizon));

    // The sun floats with its halo and its highlight, and the clouds stay above it.
    const float sunY = 0.26F * height + 0.02F * height * std::sin(seconds * 1.2F);
    if (inEllipse(x, y, 0.8F * width, sunY, 0.17F * height, 0.17F * height, 0.0F)) {
        color = mix(color, kHalo, 0.45F);
    }
    paintOutlined(color, x, y, 0.8F * width, sunY, 0.12F * height, 0.12F * height, 0.0F, outline, kSun);
    if (inEllipse(x, y, 0.78F * width, sunY - 0.02F * height, 0.07F * height, 0.07F * height, 0.0F)) {
        color = kSunLight;
    }
    for (const std::array<Circle, 3>& cloud : kClouds) {
        paintCloud(color, x, y, cloud, width, height, outline);
    }

    // Three rows of waves move at their own speeds.
    const float spacing = 0.2F * width;
    for (int row = 0; row < 3; ++row) {
        const float waveY = horizon + (0.12F + 0.12F * static_cast<float>(row)) * height;
        const float shift = std::fmod(seconds * 0.06F * width * (1.0F + 0.5F * static_cast<float>(row)) + 0.07F * width * static_cast<float>(row), spacing);
        const float start = std::fmod(x - shift + spacing * 4.0F, spacing);
        if (inCapsule(start, y, 0.01F * width, waveY, 0.1F * width, waveY, 0.007F * height)) {
            color = mix(color, kFoam, 0.6F);
        }
    }

    // The island casts its shadow on the water, under its sand, its grass and the palm with its leaves and coconuts.
    if (inEllipse(x, y, 0.36F * width, 0.69F * height, 0.24F * width, 0.05F * height, 0.0F)) {
        color = mix(color, kInk, 0.18F);
    }
    paintOutlined(color, x, y, 0.36F * width, 0.64F * height, 0.22F * width, 0.085F * height, 0.0F, outline, kSand);
    if (inEllipse(x, y, 0.24F * width, 0.675F * height, 0.05F * width, 0.013F * height, 0.0F)) {
        color = kSandLight;
    }
    paintOutlined(color, x, y, 0.36F * width, 0.6F * height, 0.15F * width, 0.05F * height, 0.0F, outline, kGrass);
    const float crownX = 0.42F * width;
    const float crownY = 0.3F * height;
    if (inCapsule(x, y, 0.38F * width, 0.6F * height, crownX, crownY, 0.015F * height)) {
        color = kTrunk;
    } else if (inCapsule(x, y, 0.38F * width, 0.6F * height, crownX, crownY, 0.015F * height + outline)) {
        color = kInk;
    }
    for (const float angle : kLeafAngles) {
        const float leafX = crownX + std::cos(angle) * 0.085F * width;
        const float leafY = crownY + std::sin(angle) * 0.085F * width;
        paintOutlined(color, x, y, leafX, leafY, 0.09F * width, 0.028F * height, angle, outline, kLeaf);
        if (inEllipse(x, y, leafX, leafY, 0.07F * width, 0.008F * height, angle)) {
            color = kLeafLight;
        }
    }
    for (const std::array<float, 2>& coconut : kCoconuts) {
        paintOutlined(color, x, y, coconut[0] * width, coconut[1] * height, 0.025F * height, 0.025F * height, 0.0F, outline, kCoconut);
    }
    return color;
}
