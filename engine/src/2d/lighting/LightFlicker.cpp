#include "haylen/2d/lighting/LightFlicker.hpp"

#include <algorithm>

#include "haylen/math/Noise2D.hpp"

namespace haylen::lighting2d {

float LightFlicker::intensity(float time, float speed, float amount, std::uint64_t seed) noexcept {
    const math::Noise2D noise(seed);
    const float wave = 0.5F + 0.5F * noise.fractal(time * speed, static_cast<float>(seed % 97U), 3);
    return 1.0F - amount * std::clamp(wave, 0.0F, 1.0F);
}

} // namespace haylen::lighting2d
