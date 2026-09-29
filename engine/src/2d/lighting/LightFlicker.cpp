#include "haylen/2d/lighting/LightFlicker.hpp"

#include <algorithm>

namespace haylen::lighting2d {

thread_local std::array<LightFlicker::Entry, LightFlicker::kCachedSeeds> LightFlicker::entries;
thread_local std::size_t LightFlicker::nextEntry = 0;

const math::Noise2D& LightFlicker::noiseFor(std::uint64_t seed) noexcept {
    for (const Entry& entry : entries) {
        if (entry.seed == seed) {
            return entry.noise;
        }
    }

    Entry& replaced = entries[nextEntry];
    nextEntry = (nextEntry + 1) % kCachedSeeds;
    replaced.seed = seed;
    replaced.noise = math::Noise2D(seed);
    return replaced.noise;
}

float LightFlicker::intensity(float time, float speed, float amount, std::uint64_t seed) noexcept {
    const float wave = 0.5F + 0.5F * noiseFor(seed).fractal(time * speed, static_cast<float>(seed % 97U), 3);
    return 1.0F - amount * std::clamp(wave, 0.0F, 1.0F);
}

} // namespace haylen::lighting2d
