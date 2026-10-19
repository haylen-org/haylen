#include "haylen/graphics/ImagePlaceholder.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>

namespace haylen::graphics {

// The text holds the counts of cosines in one byte, the largest of the higher cosines in one, the average color in three, and two bytes for each higher cosine with its three channels on a root scale, which keeps the faint ones apart.
std::string ImagePlaceholder::encode(const Image& image, Options options) {
    if (image.isEmpty()) {
        throw std::invalid_argument("An image placeholder needs an image with pixels.");
    }
    if (options.columns < 1 || options.columns > kMaxComponents || options.rows < 1 || options.rows > kMaxComponents) {
        throw std::invalid_argument("An image placeholder keeps from 1 to " + std::to_string(kMaxComponents) + " cosines on each axis.");
    }

    const int gridColumns = std::min(image.getWidth(), kGridSide);
    const int gridRows = std::min(image.getHeight(), kGridSide);
    const std::vector<Linear> grid = average(image, gridColumns, gridRows);
    std::vector<Linear> factors;
    factors.reserve(static_cast<std::size_t>(options.columns * options.rows));
    for (int j = 0; j < options.rows; ++j) {
        for (int i = 0; i < options.columns; ++i) {
            Linear factor{};
            for (int y = 0; y < gridRows; ++y) {
                const float vertical = std::cos(std::numbers::pi_v<float> * static_cast<float>(j) * (static_cast<float>(y) + 0.5F) / static_cast<float>(gridRows));
                for (int x = 0; x < gridColumns; ++x) {
                    const float basis = vertical * std::cos(std::numbers::pi_v<float> * static_cast<float>(i) * (static_cast<float>(x) + 0.5F) / static_cast<float>(gridColumns));
                    const Linear& cell = grid[static_cast<std::size_t>(y * gridColumns + x)];
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        factor[channel] += basis * cell[channel];
                    }
                }
            }
            const float scale = (i == 0 && j == 0 ? 1.0F : 2.0F) / static_cast<float>(gridColumns * gridRows);
            for (float& channel : factor) {
                channel *= scale;
            }
            factors.push_back(factor);
        }
    }

    float largest = 0.0F;
    for (std::size_t index = 1; index < factors.size(); ++index) {
        largest = std::max({largest, std::abs(factors[index][0]), std::abs(factors[index][1]), std::abs(factors[index][2])});
    }
    const int quantized = std::clamp(static_cast<int>(std::ceil(largest * 255.0F)), 1, 255);
    const float maximum = static_cast<float>(quantized) / 255.0F;

    std::vector<std::uint8_t> bytes{static_cast<std::uint8_t>((options.columns - 1) + (options.rows - 1) * kMaxComponents), static_cast<std::uint8_t>(quantized), toByte(factors[0][0]), toByte(factors[0][1]), toByte(factors[0][2])};
    for (std::size_t index = 1; index < factors.size(); ++index) {
        int packed = 0;
        for (const float channel : factors[index]) {
            const float scaled = channel / maximum;
            const float root = std::copysign(std::sqrt(std::abs(scaled)), scaled);
            packed = packed * kLevels + std::clamp(static_cast<int>(std::lround(root * static_cast<float>(kLevels / 2) + static_cast<float>(kLevels / 2))), 0, kLevels - 1);
        }
        bytes.push_back(static_cast<std::uint8_t>(packed >> 8));
        bytes.push_back(static_cast<std::uint8_t>(packed & 0xFF));
    }
    return toText(bytes);
}

// Every count, length and level of the text is checked before it is used, since a placeholder may come from anywhere, such as a server.
Image ImagePlaceholder::decode(std::string_view code, int width, int height) {
    if (width < 1 || width > kMaxSize || height < 1 || height > kMaxSize) {
        throw std::invalid_argument("An image placeholder decodes to a size from 1 to " + std::to_string(kMaxSize) + " pixels on each side.");
    }
    const std::vector<std::uint8_t> bytes = fromText(code);
    if (bytes.size() < 5 || bytes[0] >= kMaxComponents * kMaxComponents) {
        throw std::invalid_argument("The text is not an image placeholder.");
    }
    const int columns = bytes[0] % kMaxComponents + 1;
    const int rows = bytes[0] / kMaxComponents + 1;
    if (bytes.size() != 5 + static_cast<std::size_t>(columns * rows - 1) * 2 || bytes[1] == 0) {
        throw std::invalid_argument("The text is not an image placeholder.");
    }

    const float maximum = static_cast<float>(bytes[1]) / 255.0F;
    std::vector<Linear> factors{{toLinear(bytes[2]), toLinear(bytes[3]), toLinear(bytes[4])}};
    for (std::size_t offset = 5; offset < bytes.size(); offset += 2) {
        int packed = bytes[offset] << 8 | bytes[offset + 1];
        if (packed >= kLevels * kLevels * kLevels) {
            throw std::invalid_argument("The text is not an image placeholder.");
        }
        Linear factor{};
        for (int channel = 2; channel >= 0; --channel) {
            const float root = (static_cast<float>(packed % kLevels) - static_cast<float>(kLevels / 2)) / static_cast<float>(kLevels / 2);
            factor[static_cast<std::size_t>(channel)] = std::copysign(root * root, root) * maximum;
            packed /= kLevels;
        }
        factors.push_back(factor);
    }

    Image image(width, height);
    const std::span<std::uint8_t> pixels = image.getPixels();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Linear color{};
            for (int j = 0; j < rows; ++j) {
                const float vertical = std::cos(std::numbers::pi_v<float> * static_cast<float>(j) * (static_cast<float>(y) + 0.5F) / static_cast<float>(height));
                for (int i = 0; i < columns; ++i) {
                    const float basis = vertical * std::cos(std::numbers::pi_v<float> * static_cast<float>(i) * (static_cast<float>(x) + 0.5F) / static_cast<float>(width));
                    const Linear& factor = factors[static_cast<std::size_t>(j * columns + i)];
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        color[channel] += basis * factor[channel];
                    }
                }
            }
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)) * 4;
            pixels[offset] = toByte(color[0]);
            pixels[offset + 1] = toByte(color[1]);
            pixels[offset + 2] = toByte(color[2]);
            pixels[offset + 3] = 255;
        }
    }
    return image;
}

float ImagePlaceholder::toLinear(std::uint8_t channel) noexcept {
    const float value = static_cast<float>(channel) / 255.0F;
    return value <= 0.04045F ? value / 12.92F : std::pow((value + 0.055F) / 1.055F, 2.4F);
}

std::uint8_t ImagePlaceholder::toByte(float linear) noexcept {
    const float value = std::clamp(linear, 0.0F, 1.0F);
    const float encoded = value <= 0.0031308F ? value * 12.92F : 1.055F * std::pow(value, 1.0F / 2.4F) - 0.055F;
    return static_cast<std::uint8_t>(std::lround(encoded * 255.0F));
}

// Each cell of the grid averages the pixels it covers in linear light, with transparent pixels counting as black.
std::vector<ImagePlaceholder::Linear> ImagePlaceholder::average(const Image& image, int columns, int rows) {
    std::vector<Linear> grid(static_cast<std::size_t>(columns * rows));
    std::vector<int> counts(grid.size(), 0);
    const std::span<const std::uint8_t> pixels = image.getPixels();
    for (int y = 0; y < image.getHeight(); ++y) {
        const int row = y * rows / image.getHeight();
        for (int x = 0; x < image.getWidth(); ++x) {
            const std::size_t cell = static_cast<std::size_t>(row * columns + x * columns / image.getWidth());
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.getWidth()) + static_cast<std::size_t>(x)) * 4;
            const float alpha = static_cast<float>(pixels[offset + 3]) / 255.0F;
            for (std::size_t channel = 0; channel < 3; ++channel) {
                grid[cell][channel] += toLinear(pixels[offset + channel]) * alpha;
            }
            ++counts[cell];
        }
    }
    for (std::size_t cell = 0; cell < grid.size(); ++cell) {
        for (float& channel : grid[cell]) {
            channel /= static_cast<float>(std::max(counts[cell], 1));
        }
    }
    return grid;
}

std::string ImagePlaceholder::toText(const std::vector<std::uint8_t>& bytes) {
    std::string text;
    text.reserve((bytes.size() * 8 + 5) / 6);
    unsigned buffer = 0;
    int bits = 0;
    for (const std::uint8_t byte : bytes) {
        buffer = (buffer << 8U) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            text += kAlphabet[(buffer >> static_cast<unsigned>(bits)) & 0x3FU];
        }
    }
    if (bits > 0) {
        text += kAlphabet[(buffer << static_cast<unsigned>(6 - bits)) & 0x3FU];
    }
    return text;
}

// A placeholder holds at most a few hundred bytes, so a longer text is refused before it is read.
std::vector<std::uint8_t> ImagePlaceholder::fromText(std::string_view text) {
    constexpr std::size_t kMaxBytes = 5 + (kMaxComponents * kMaxComponents - 1) * 2;
    if (text.size() > (kMaxBytes * 8 + 5) / 6) {
        throw std::invalid_argument("The text is not an image placeholder.");
    }
    std::vector<std::uint8_t> bytes;
    bytes.reserve(text.size() * 6 / 8);
    unsigned buffer = 0;
    int bits = 0;
    for (const char character : text) {
        const std::size_t value = kAlphabet.find(character);
        if (value == std::string_view::npos) {
            throw std::invalid_argument("The text is not an image placeholder.");
        }
        buffer = (buffer << 6U) | static_cast<unsigned>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            bytes.push_back(static_cast<std::uint8_t>((buffer >> static_cast<unsigned>(bits)) & 0xFFU));
        }
    }
    return bytes;
}

} // namespace haylen::graphics
