#include "haylen/graphics/Image.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

namespace haylen::graphics {

std::size_t Image::byteCount(int columns, int rows) {
    if (columns < 0 || rows < 0) {
        throw std::invalid_argument("Image dimensions cannot be negative.");
    }

    // The byte count must fit in a pointer difference, which also keeps it from overflowing on 32-bit platforms.
    const auto columnCount = static_cast<std::size_t>(columns);
    const auto rowCount = static_cast<std::size_t>(rows);
    if (rowCount != 0 && columnCount > static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) / 4U / rowCount) {
        throw std::invalid_argument("Image dimensions are too large to hold in memory.");
    }
    return columnCount * rowCount * 4U;
}

Image::Image(int imageWidth, int imageHeight, math::Color fill) : width(imageWidth), height(imageHeight), pixels(byteCount(imageWidth, imageHeight)) {
    const std::uint32_t packed = fill.toRgba8();
    for (std::size_t offset = 0; offset < pixels.size(); offset += 4) {
        pixels[offset + 0] = static_cast<std::uint8_t>(packed & 0xFFU);
        pixels[offset + 1] = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
        pixels[offset + 2] = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
        pixels[offset + 3] = static_cast<std::uint8_t>((packed >> 24U) & 0xFFU);
    }
}

Image::Image(int imageWidth, int imageHeight, std::vector<std::uint8_t> data) : width(imageWidth), height(imageHeight), pixels(std::move(data)) {
    if (pixels.size() != byteCount(imageWidth, imageHeight)) {
        throw std::invalid_argument("Image pixel data does not match its dimensions.");
    }
}

Image Image::decode(std::span<const std::uint8_t> encoded) {
    int decodedWidth = 0;
    int decodedHeight = 0;
    int channels = 0;
    stbi_uc* decoded = stbi_load_from_memory(encoded.data(), static_cast<int>(encoded.size()), &decodedWidth, &decodedHeight, &channels, STBI_rgb_alpha);
    if (decoded == nullptr) {
        throw std::runtime_error(std::string("The image could not be decoded, and the decoder reported \"") + stbi_failure_reason() + "\".");
    }

    std::vector<std::uint8_t> data(decoded, decoded + byteCount(decodedWidth, decodedHeight));
    stbi_image_free(decoded);
    return Image(decodedWidth, decodedHeight, std::move(data));
}

std::vector<std::uint8_t> Image::encodePng() const {
    if (pixels.empty()) {
        throw std::invalid_argument("An empty image has no PNG file.");
    }
    std::vector<std::uint8_t> encoded;
    // clang-format off
    stbi_write_png_to_func([](void* context, void* data, int size) {
        auto* target = static_cast<std::vector<std::uint8_t>*>(context);
        target->insert(target->end(), static_cast<const std::uint8_t*>(data), static_cast<const std::uint8_t*>(data) + size);
    }, &encoded, width, height, 4, pixels.data(), width * 4);
    // clang-format on
    return encoded;
}

// Rows are swapped when the backend stores the bottom row first, and colors are divided by their alpha.
Image Image::fromRendered(std::vector<std::uint8_t> data, int columns, int rows, bool bottomUp) {
    const auto rowBytes = static_cast<std::size_t>(columns) * 4U;
    if (bottomUp) {
        for (int row = 0; row < rows / 2; ++row) {
            std::swap_ranges(data.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row) * rowBytes), data.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(row + 1) * rowBytes), data.begin() + static_cast<std::ptrdiff_t>(static_cast<std::size_t>(rows - 1 - row) * rowBytes));
        }
    }
    for (std::size_t offset = 0; offset < data.size(); offset += 4) {
        const unsigned alpha = data[offset + 3];
        if (alpha == 0U || alpha == 255U) {
            continue;
        }
        for (std::size_t channel = 0; channel < 3; ++channel) {
            data[offset + channel] = static_cast<std::uint8_t>(std::min(255U, (static_cast<unsigned>(data[offset + channel]) * 255U + alpha / 2U) / alpha));
        }
    }
    return Image(columns, rows, std::move(data));
}

Image Image::decodeFor(std::span<const std::uint8_t> encoded, Texture::Options options) {
    Image image = decode(encoded);
    if (options.filter == Texture::Filter::Linear) {
        image.bleed();
    }
    if (options.mipmaps) {
        image.buildMipmaps();
    }
    return image;
}

// The far edges add up in 64 bits, so an area that reaches past the largest int clips instead of wrapping around.
Image::Area Image::clip(const Area& area, int columns, int rows) noexcept {
    const int left = std::max(area.x, 0);
    const int top = std::max(area.y, 0);
    const auto right = static_cast<int>(std::min<std::int64_t>(std::int64_t{area.x} + area.width, columns));
    const auto bottom = static_cast<int>(std::min<std::int64_t>(std::int64_t{area.y} + area.height, rows));
    return {left, top, std::max(right - left, 0), std::max(bottom - top, 0)};
}

void Image::fill(const Area& area, math::Color color) noexcept {
    const Area inside = clip(area, width, height);
    const std::uint32_t packed = color.toRgba8();
    for (int row = inside.y; row < inside.y + inside.height; ++row) {
        for (int column = inside.x; column < inside.x + inside.width; ++column) {
            std::memcpy(&pixels[static_cast<std::size_t>(row * width + column) * 4U], &packed, 4);
        }
    }
    mipmaps.clear();
}

// The copy is clipped to the source area inside the source and to the destination inside this image, and blending mixes straight colors by the alpha of the source over the alpha of the destination.
void Image::blit(const Image& source, const Area& from, int x, int y, BlitMode mode) {
    if (&source == this) {
        const Image snapshot = *this;
        blit(snapshot, from, x, y, mode);
        return;
    }
    const Area region = clip(from, source.width, source.height);
    const int shiftX = x + region.x - from.x;
    const int shiftY = y + region.y - from.y;
    const Area target = clip({shiftX, shiftY, region.width, region.height}, width, height);
    for (int row = 0; row < target.height; ++row) {
        const int sourceRow = region.y + (target.y - shiftY) + row;
        const int sourceColumn = region.x + (target.x - shiftX);
        const std::uint8_t* in = &source.pixels[static_cast<std::size_t>(sourceRow * source.width + sourceColumn) * 4U];
        std::uint8_t* out = &pixels[static_cast<std::size_t>((target.y + row) * width + target.x) * 4U];
        if (mode == BlitMode::Replace) {
            std::memcpy(out, in, static_cast<std::size_t>(target.width) * 4U);
            continue;
        }
        for (int column = 0; column < target.width; ++column, in += 4, out += 4) {
            const float alpha = static_cast<float>(in[3]) / 255.0F;
            const float below = static_cast<float>(out[3]) / 255.0F * (1.0F - alpha);
            const float total = alpha + below;
            for (int channel = 0; channel < 3; ++channel) {
                out[channel] = total > 0.0F ? static_cast<std::uint8_t>(std::lround((static_cast<float>(in[channel]) * alpha + static_cast<float>(out[channel]) * below) / total)) : in[channel];
            }
            out[3] = static_cast<std::uint8_t>(std::lround(total * 255.0F));
        }
    }
    mipmaps.clear();
}

Image Image::crop(const Area& area) const {
    if (area.x < 0 || area.y < 0 || area.width < 0 || area.height < 0 || area.x > width || area.y > height || area.width > width - area.x || area.height > height - area.y) {
        throw std::out_of_range("The area " + std::to_string(area.x) + ", " + std::to_string(area.y) + ", " + std::to_string(area.width) + " by " + std::to_string(area.height) + " is not inside the image of " + std::to_string(width) + " by " + std::to_string(height) + " pixels.");
    }
    Image result(area.width, area.height);
    result.blit(*this, area, 0, 0);
    return result;
}

void Image::extrude(const Area& area, int amount) noexcept {
    const Area inside = clip(area, width, height);
    if (inside.width == 0 || inside.height == 0 || amount <= 0) {
        return;
    }
    const Area grown = clip({inside.x - amount, inside.y - amount, inside.width + amount * 2, inside.height + amount * 2}, width, height);
    for (int row = grown.y; row < grown.y + grown.height; ++row) {
        const int sourceRow = std::clamp(row, inside.y, inside.y + inside.height - 1);
        for (int column = grown.x; column < grown.x + grown.width; ++column) {
            const int sourceColumn = std::clamp(column, inside.x, inside.x + inside.width - 1);
            if (sourceRow != row || sourceColumn != column) {
                std::memcpy(&pixels[static_cast<std::size_t>(row * width + column) * 4U], &pixels[static_cast<std::size_t>(sourceRow * width + sourceColumn) * 4U], 4);
            }
        }
    }
    mipmaps.clear();
}

// A search from every pixel that shows at once reaches each transparent pixel first from the nearest one, which lends it its color, one ring of pixels at a time.
void Image::bleed() {
    const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> reached(count, 0);
    std::vector<std::uint32_t> queue;
    for (std::size_t index = 0; index < count; ++index) {
        if (pixels[index * 4U + 3U] > 0) {
            reached[index] = 1;
            queue.push_back(static_cast<std::uint32_t>(index));
        }
    }
    std::size_t ringEnd = queue.size();
    int ring = 0;
    for (std::size_t head = 0; head < queue.size(); ++head) {
        if (head == ringEnd) {
            ringEnd = queue.size();
            if (++ring == kBleedReach) {
                break;
            }
        }
        const std::uint32_t index = queue[head];
        const int x = static_cast<int>(index % static_cast<std::uint32_t>(width));
        const int y = static_cast<int>(index / static_cast<std::uint32_t>(width));
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                const int nx = x + dx;
                const int ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= width || ny >= height) {
                    continue;
                }
                const auto next = static_cast<std::size_t>(ny * width + nx);
                if (reached[next] != 0) {
                    continue;
                }
                reached[next] = 1;
                std::memcpy(&pixels[next * 4U], &pixels[static_cast<std::size_t>(index) * 4U], 3);
                queue.push_back(static_cast<std::uint32_t>(next));
            }
        }
    }
    mipmaps.clear();
}

Image::Area Image::getVisibleArea() const noexcept {
    int left = width;
    int top = height;
    int right = -1;
    int bottom = -1;
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            if (pixels[static_cast<std::size_t>(row * width + column) * 4U + 3U] > 0) {
                left = std::min(left, column);
                right = std::max(right, column);
                top = std::min(top, row);
                bottom = std::max(bottom, row);
            }
        }
    }
    return right < 0 ? Area{} : Area{left, top, right - left + 1, bottom - top + 1};
}

Image Image::halved() const {
    const int halfWidth = std::max(1, width / 2);
    const int halfHeight = std::max(1, height / 2);
    Image result(halfWidth, halfHeight);
    for (int row = 0; row < halfHeight; ++row) {
        for (int column = 0; column < halfWidth; ++column) {
            std::array<float, 3> weighted{};
            std::array<float, 3> plain{};
            float alpha = 0.0F;
            for (int dy = 0; dy < 2; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    const int x = std::min(column * 2 + dx, width - 1);
                    const int y = std::min(row * 2 + dy, height - 1);
                    const std::uint8_t* in = &pixels[static_cast<std::size_t>(y * width + x) * 4U];
                    const float weight = static_cast<float>(in[3]);
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        weighted[channel] += static_cast<float>(in[channel]) * weight;
                        plain[channel] += static_cast<float>(in[channel]);
                    }
                    alpha += weight;
                }
            }
            std::uint8_t* out = &result.pixels[static_cast<std::size_t>(row * halfWidth + column) * 4U];
            for (std::size_t channel = 0; channel < 3; ++channel) {
                out[channel] = static_cast<std::uint8_t>(std::lround(alpha > 0.0F ? weighted[channel] / alpha : plain[channel] / 4.0F));
            }
            out[3] = static_cast<std::uint8_t>(std::lround(alpha / 4.0F));
        }
    }
    return result;
}

void Image::buildMipmaps() {
    mipmaps.clear();
    std::size_t levels = 0;
    for (int size = std::max(width, height); size > 1; size /= 2) {
        ++levels;
    }
    mipmaps.reserve(levels);
    for (std::size_t level = 0; level < levels; ++level) {
        mipmaps.push_back(level == 0 ? halved() : mipmaps.back().halved());
    }
}

math::Color Image::getPixel(int x, int y) const noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return math::Color::transparent();
    }
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    return math::Color::fromRgba8(pixels[offset], pixels[offset + 1], pixels[offset + 2], pixels[offset + 3]);
}

void Image::setPixel(int x, int y, math::Color color) noexcept {
    if (x < 0 || y < 0 || x >= width || y >= height) {
        return;
    }
    mipmaps.clear();
    const auto offset = static_cast<std::size_t>((y * width + x) * 4);
    const std::uint32_t packed = color.toRgba8();
    pixels[offset + 0] = static_cast<std::uint8_t>(packed & 0xFFU);
    pixels[offset + 1] = static_cast<std::uint8_t>((packed >> 8U) & 0xFFU);
    pixels[offset + 2] = static_cast<std::uint8_t>((packed >> 16U) & 0xFFU);
    pixels[offset + 3] = static_cast<std::uint8_t>((packed >> 24U) & 0xFFU);
}

} // namespace haylen::graphics
