#include "haylen/graphics/ImagePalette.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>

namespace haylen::graphics {

// Each box is a range of the samples. The box whose widest channel spans the most, weighed by how many samples it holds, splits at the median of that channel, until the palette has its colors or no box holds two different colors.
std::vector<ImagePalette::Swatch> ImagePalette::extract(const Image& image, int count) {
    if (count < 1 || count > kMaxColors) {
        throw std::invalid_argument("A palette has from 1 to " + std::to_string(kMaxColors) + " colors, not " + std::to_string(count) + ".");
    }
    std::vector<Sample> samples = sample(image);
    if (samples.empty()) {
        return {};
    }

    std::vector<std::span<Sample>> boxes{std::span(samples)};
    while (static_cast<int>(boxes.size()) < count) {
        auto widest = boxes.end();
        long best = 0;
        for (auto box = boxes.begin(); box != boxes.end(); ++box) {
            const long score = static_cast<long>(channelRange(*box, widestChannel(*box))) * static_cast<long>(box->size());
            if (score > best) {
                best = score;
                widest = box;
            }
        }
        if (widest == boxes.end()) {
            break;
        }
        // The box splits below the median value of the channel, or above it when the median is the lowest value, so the samples of one value never end up in both halves.
        const std::span<Sample> box = *widest;
        const auto channel = static_cast<std::size_t>(widestChannel(box));
        const auto value = [channel](const Sample& sample) { return sample[channel]; };
        std::ranges::nth_element(box, box.begin() + static_cast<std::ptrdiff_t>(box.size() / 2), {}, value);
        const std::uint8_t median = box[box.size() / 2][channel];
        auto split = std::ranges::partition(box, [&](const Sample& sample) { return value(sample) < median; }).begin();
        if (split == box.begin()) {
            split = std::ranges::partition(box, [&](const Sample& sample) { return value(sample) <= median; }).begin();
        }
        const auto half = static_cast<std::size_t>(split - box.begin());
        *widest = box.first(half);
        boxes.push_back(box.subspan(half));
    }

    std::vector<Swatch> palette;
    palette.reserve(boxes.size());
    for (const std::span<Sample> box : boxes) {
        palette.push_back({.color = average(box), .population = static_cast<float>(box.size()) / static_cast<float>(samples.size())});
    }
    std::ranges::stable_sort(palette, std::ranges::greater{}, &Swatch::population);
    return palette;
}

// Each target takes the color of the palette that fits its ranges best, closest to its lightness above all and then to its saturation, with a common color winning a close call, and no color serves two targets.
ImagePalette::Swatches ImagePalette::choose(std::span<const Swatch> palette) {
    Swatches swatches;
    if (palette.empty()) {
        return swatches;
    }
    swatches.dominant = palette.front().color;
    std::vector<bool> used(palette.size(), false);
    swatches.vibrant = find(palette, {.lightness = 0.5F, .minimumLightness = 0.3F, .maximumLightness = 0.7F, .saturation = 1.0F, .minimumSaturation = 0.35F}, used);
    swatches.lightVibrant = find(palette, {.lightness = 0.74F, .minimumLightness = 0.55F, .saturation = 1.0F, .minimumSaturation = 0.35F}, used);
    swatches.darkVibrant = find(palette, {.lightness = 0.26F, .maximumLightness = 0.45F, .saturation = 1.0F, .minimumSaturation = 0.35F}, used);
    swatches.muted = find(palette, {.lightness = 0.5F, .minimumLightness = 0.3F, .maximumLightness = 0.7F, .saturation = 0.3F, .maximumSaturation = 0.4F}, used);
    swatches.lightMuted = find(palette, {.lightness = 0.74F, .minimumLightness = 0.55F, .saturation = 0.3F, .maximumSaturation = 0.4F}, used);
    swatches.darkMuted = find(palette, {.lightness = 0.26F, .maximumLightness = 0.45F, .saturation = 0.3F, .maximumSaturation = 0.4F}, used);
    return swatches;
}

// Pixels that are mostly transparent show nothing of the image, so they stay out of its colors.
std::vector<ImagePalette::Sample> ImagePalette::sample(const Image& image) {
    std::vector<Sample> samples;
    if (image.isEmpty()) {
        return samples;
    }
    const int step = std::max(1, (std::max(image.getWidth(), image.getHeight()) + kSampleSide - 1) / kSampleSide);
    const std::span<const std::uint8_t> pixels = image.getPixels();
    samples.reserve(static_cast<std::size_t>(((image.getWidth() + step - 1) / step) * ((image.getHeight() + step - 1) / step)));
    for (int y = 0; y < image.getHeight(); y += step) {
        for (int x = 0; x < image.getWidth(); x += step) {
            const std::size_t offset = (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.getWidth()) + static_cast<std::size_t>(x)) * 4;
            if (pixels[offset + 3] >= 128) {
                samples.push_back({pixels[offset], pixels[offset + 1], pixels[offset + 2]});
            }
        }
    }
    return samples;
}

int ImagePalette::widestChannel(std::span<const Sample> samples) noexcept {
    int widest = 0;
    for (int channel = 1; channel < 3; ++channel) {
        if (channelRange(samples, channel) > channelRange(samples, widest)) {
            widest = channel;
        }
    }
    return widest;
}

int ImagePalette::channelRange(std::span<const Sample> samples, int channel) noexcept {
    const auto [low, high] = std::ranges::minmax(samples, {}, [channel](const Sample& value) { return value[static_cast<std::size_t>(channel)]; });
    return high[static_cast<std::size_t>(channel)] - low[static_cast<std::size_t>(channel)];
}

math::Color ImagePalette::average(std::span<const Sample> samples) noexcept {
    std::array<double, 3> sums{};
    for (const Sample& value : samples) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            sums[channel] += value[channel];
        }
    }
    const double scale = 1.0 / (255.0 * static_cast<double>(samples.size()));
    return {static_cast<float>(sums[0] * scale), static_cast<float>(sums[1] * scale), static_cast<float>(sums[2] * scale), 1.0F};
}

std::array<float, 2> ImagePalette::lightnessAndSaturation(const math::Color& color) noexcept {
    const float high = std::max({color.r, color.g, color.b});
    const float low = std::min({color.r, color.g, color.b});
    const float lightness = (high + low) * 0.5F;
    const float spread = 1.0F - std::abs(2.0F * lightness - 1.0F);
    return {lightness, high == low || spread <= 0.0F ? 0.0F : (high - low) / spread};
}

std::optional<math::Color> ImagePalette::find(std::span<const Swatch> palette, const Target& target, std::vector<bool>& used) {
    const float mostCommon = palette.front().population;
    std::optional<std::size_t> best;
    float bestScore = 0.0F;
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto [lightness, saturation] = lightnessAndSaturation(palette[index].color);
        if (used[index] || lightness < target.minimumLightness || lightness > target.maximumLightness || saturation < target.minimumSaturation || saturation > target.maximumSaturation) {
            continue;
        }
        const float score = (1.0F - std::abs(lightness - target.lightness)) * 6.0F + (1.0F - std::abs(saturation - target.saturation)) * 3.0F + palette[index].population / mostCommon;
        if (!best || score > bestScore) {
            best = index;
            bestScore = score;
        }
    }
    if (!best) {
        return std::nullopt;
    }
    used[*best] = true;
    return palette[*best].color;
}

} // namespace haylen::graphics
