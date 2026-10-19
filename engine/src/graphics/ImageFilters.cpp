#include "haylen/graphics/ImageFilters.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace haylen::graphics {

// Three box blurs whose widths add up to the variance of the Gaussian come within a few percent of it, at a cost that does not grow with the radius.
void ImageFilters::blur(Image& image, float radius) {
    if (!(radius >= 0.0F) || radius > kMaxRadius) {
        throw std::invalid_argument("A blur has a radius from 0 to " + std::to_string(static_cast<int>(kMaxRadius)) + " pixels.");
    }
    if (radius == 0.0F || image.isEmpty()) {
        return;
    }
    Pixels pixels = premultiplied(image);
    Pixels scratch(pixels.size());
    for (const int box : boxRadii(radius)) {
        boxBlur(pixels, scratch, image.getWidth(), image.getHeight(), box);
    }
    store(image, pixels);
}

void ImageFilters::sharpen(Image& image, float amount) {
    if (!(amount >= 0.0F) || !std::isfinite(amount)) {
        throw std::invalid_argument("A sharpening has an amount of 0 or more.");
    }
    if (amount == 0.0F || image.isEmpty()) {
        return;
    }
    const Pixels original = premultiplied(image);
    Pixels blurred = original;
    Pixels scratch(blurred.size());
    boxBlur(blurred, scratch, image.getWidth(), image.getHeight(), 1);
    for (std::size_t index = 0; index < blurred.size(); ++index) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            blurred[index][channel] = std::clamp(original[index][channel] + (original[index][channel] - blurred[index][channel]) * amount, 0.0F, original[index][3]);
        }
        blurred[index][3] = original[index][3];
    }
    store(image, blurred);
}

void ImageFilters::adjust(Image& image, const Adjustment& adjustment) noexcept {
    const std::span<std::uint8_t> bytes = image.getPixels();
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        math::Color color = math::Color::fromRgba8(bytes[offset], bytes[offset + 1], bytes[offset + 2], bytes[offset + 3]);
        if (adjustment.hue != 0.0F) {
            const math::Color::Hsv hsv = color.toHsv();
            color = math::Color::fromHsv(hsv.hue + adjustment.hue, hsv.saturation, hsv.value, hsv.alpha);
        }
        const float gray = 0.2126F * color.r + 0.7152F * color.g + 0.0722F * color.b;
        for (float* channel : {&color.r, &color.g, &color.b}) {
            *channel = gray + (*channel - gray) * adjustment.saturation;
            *channel = 0.5F + (*channel - 0.5F) * adjustment.contrast + adjustment.brightness;
        }
        bytes[offset] = toByte(color.r);
        bytes[offset + 1] = toByte(color.g);
        bytes[offset + 2] = toByte(color.b);
    }
}

void ImageFilters::grayscale(Image& image, float amount) noexcept {
    adjust(image, {.saturation = 1.0F - std::clamp(amount, 0.0F, 1.0F)});
}

void ImageFilters::invert(Image& image) noexcept {
    const std::span<std::uint8_t> bytes = image.getPixels();
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            bytes[offset + channel] = static_cast<std::uint8_t>(255 - bytes[offset + channel]);
        }
    }
}

void ImageFilters::tint(Image& image, math::Color color, float amount) noexcept {
    const float share = std::clamp(amount, 0.0F, 1.0F) * std::clamp(color.a, 0.0F, 1.0F);
    const std::array<float, 3> target{color.r * 255.0F, color.g * 255.0F, color.b * 255.0F};
    const std::span<std::uint8_t> bytes = image.getPixels();
    for (std::size_t offset = 0; offset < bytes.size(); offset += 4) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const float value = static_cast<float>(bytes[offset + channel]);
            bytes[offset + channel] = static_cast<std::uint8_t>(std::clamp(std::lround(value + (target[channel] - value) * share), 0L, 255L));
        }
    }
}

void ImageFilters::pixelate(Image& image, int size) {
    if (size < 1) {
        throw std::invalid_argument("A pixelation has squares of 1 pixel or more.");
    }
    if (size == 1 || image.isEmpty()) {
        return;
    }
    Pixels pixels = premultiplied(image);
    const int width = image.getWidth();
    const int height = image.getHeight();
    for (int top = 0; top < height; top += size) {
        for (int left = 0; left < width; left += size) {
            const int right = std::min(left + size, width);
            const int bottom = std::min(top + size, height);
            std::array<float, 4> sum{};
            for (int y = top; y < bottom; ++y) {
                for (int x = left; x < right; ++x) {
                    for (std::size_t channel = 0; channel < 4; ++channel) {
                        sum[channel] += pixels[static_cast<std::size_t>(y * width + x)][channel];
                    }
                }
            }
            const float count = static_cast<float>((right - left) * (bottom - top));
            for (float& channel : sum) {
                channel /= count;
            }
            for (int y = top; y < bottom; ++y) {
                for (int x = left; x < right; ++x) {
                    pixels[static_cast<std::size_t>(y * width + x)] = sum;
                }
            }
        }
    }
    store(image, pixels);
}

// The distance of every pixel to the nearest pixel that shows comes from two passes of a chamfer over the image, close enough to the true distance for an outline, and the pixels within the thickness take the color under what they already show.
void ImageFilters::outline(Image& image, math::Color color, float thickness) {
    if (!(thickness >= 0.0F) || thickness > kMaxThickness) {
        throw std::invalid_argument("An outline has a thickness from 0 to " + std::to_string(static_cast<int>(kMaxThickness)) + " pixels.");
    }
    if (thickness == 0.0F || image.isEmpty()) {
        return;
    }
    const int width = image.getWidth();
    const int height = image.getHeight();
    const std::span<std::uint8_t> bytes = image.getPixels();
    constexpr float kFar = std::numeric_limits<float>::max() / 4.0F;
    constexpr float kDiagonal = std::numbers::sqrt2_v<float>;
    std::vector<float> distances(static_cast<std::size_t>(width * height));
    for (std::size_t index = 0; index < distances.size(); ++index) {
        distances[index] = bytes[index * 4 + 3] >= 128 ? 0.0F : kFar;
    }
    // clang-format off
    const auto relax = [&distances, width, height](int x, int y, int dx, int dy, float step) {
        const int nx = x + dx;
        const int ny = y + dy;
        if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
            return;
        }
        float& here = distances[static_cast<std::size_t>(y * width + x)];
        here = std::min(here, distances[static_cast<std::size_t>(ny * width + nx)] + step);
    };
    // clang-format on
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            relax(x, y, -1, 0, 1.0F);
            relax(x, y, 0, -1, 1.0F);
            relax(x, y, -1, -1, kDiagonal);
            relax(x, y, 1, -1, kDiagonal);
        }
    }
    for (int y = height - 1; y >= 0; --y) {
        for (int x = width - 1; x >= 0; --x) {
            relax(x, y, 1, 0, 1.0F);
            relax(x, y, 0, 1, 1.0F);
            relax(x, y, 1, 1, kDiagonal);
            relax(x, y, -1, 1, kDiagonal);
        }
    }

    for (std::size_t index = 0; index < distances.size(); ++index) {
        const float coverage = std::clamp(thickness + 1.0F - distances[index], 0.0F, 1.0F) * std::clamp(color.a, 0.0F, 1.0F);
        if (coverage <= 0.0F) {
            continue;
        }
        std::uint8_t* pixel = &bytes[index * 4];
        const float alpha = static_cast<float>(pixel[3]) / 255.0F;
        const float under = coverage * (1.0F - alpha);
        const float total = alpha + under;
        const std::array<float, 3> outlineColor{color.r, color.g, color.b};
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const float mixed = (static_cast<float>(pixel[channel]) / 255.0F * alpha + outlineColor[channel] * under) / total;
            pixel[channel] = toByte(mixed);
        }
        pixel[3] = toByte(total);
    }
}

std::uint8_t ImageFilters::toByte(float value) noexcept {
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
}

ImageFilters::Pixels ImageFilters::premultiplied(const Image& image) {
    const std::span<const std::uint8_t> bytes = image.getPixels();
    Pixels pixels(bytes.size() / 4);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float alpha = static_cast<float>(bytes[index * 4 + 3]) / 255.0F;
        pixels[index] = {static_cast<float>(bytes[index * 4]) / 255.0F * alpha, static_cast<float>(bytes[index * 4 + 1]) / 255.0F * alpha, static_cast<float>(bytes[index * 4 + 2]) / 255.0F * alpha, alpha};
    }
    return pixels;
}

void ImageFilters::store(Image& image, const Pixels& pixels) noexcept {
    const std::span<std::uint8_t> bytes = image.getPixels();
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const float alpha = pixels[index][3];
        const float scale = alpha > 0.0F ? 1.0F / alpha : 0.0F;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            bytes[index * 4 + channel] = toByte(pixels[index][channel] * scale);
        }
        bytes[index * 4 + 3] = toByte(alpha);
    }
}

// A running sum along each row and then each column adds the pixel that enters the window and takes away the one that leaves it, with the pixels at the edges repeated past them.
void ImageFilters::boxBlur(Pixels& pixels, Pixels& scratch, int width, int height, int radius) {
    if (radius <= 0) {
        return;
    }
    const float scale = 1.0F / static_cast<float>(2 * radius + 1);
    // clang-format off
    const auto pass = [radius, scale](const Pixels& source, Pixels& target, int count, int lines, int stride, int step) {
        for (int line = 0; line < lines; ++line) {
            const auto at = [&source, line, stride, step, count](int position) -> const std::array<float, 4>& {
                return source[static_cast<std::size_t>(line * stride + std::clamp(position, 0, count - 1) * step)];
            };
            std::array<float, 4> sum{};
            for (int position = -radius; position <= radius; ++position) {
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    sum[channel] += at(position)[channel];
                }
            }
            for (int position = 0; position < count; ++position) {
                std::array<float, 4>& out = target[static_cast<std::size_t>(line * stride + position * step)];
                for (std::size_t channel = 0; channel < 4; ++channel) {
                    out[channel] = sum[channel] * scale;
                    sum[channel] += at(position + radius + 1)[channel] - at(position - radius)[channel];
                }
            }
        }
    };
    // clang-format on
    pass(pixels, scratch, width, height, width, 1);
    pass(scratch, pixels, height, width, 1, width);
}

std::array<int, 3> ImageFilters::boxRadii(float sigma) noexcept {
    constexpr int kBoxes = 3;
    const float ideal = std::sqrt(12.0F * sigma * sigma / kBoxes + 1.0F);
    int lower = static_cast<int>(std::floor(ideal));
    if (lower % 2 == 0) {
        --lower;
    }
    const int upper = lower + 2;
    const float split = (12.0F * sigma * sigma - static_cast<float>(kBoxes * lower * lower + 4 * kBoxes * lower + 3 * kBoxes)) / static_cast<float>(-4 * lower - 4);
    const int lowerCount = static_cast<int>(std::lround(split));
    std::array<int, 3> radii{};
    for (int box = 0; box < kBoxes; ++box) {
        radii[static_cast<std::size_t>(box)] = ((box < lowerCount ? lower : upper) - 1) / 2;
    }
    return radii;
}

} // namespace haylen::graphics
