#include "audio/Freeverb.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::audio {

Freeverb::Freeverb(std::uint32_t sampleRate) {
    const float ratio = static_cast<float>(sampleRate) / kTuningRate;
    for (std::size_t index = 0; index < kCombTuning.size(); ++index) {
        combsLeft[index].buffer.assign(scaled(kCombTuning[index], ratio), 0.0F);
        combsRight[index].buffer.assign(scaled(kCombTuning[index] + kStereoSpread, ratio), 0.0F);
    }
    for (std::size_t index = 0; index < kAllpassTuning.size(); ++index) {
        allpassesLeft[index].buffer.assign(scaled(kAllpassTuning[index], ratio), 0.0F);
        allpassesRight[index].buffer.assign(scaled(kAllpassTuning[index] + kStereoSpread, ratio), 0.0F);
    }
}

void Freeverb::setRoomSize(float value) noexcept {
    feedback = value * kRoomScale + kRoomOffset;
}

void Freeverb::setDamping(float value) noexcept {
    damping = value * kDampingScale;
}

void Freeverb::setMix(float wet, float dryLevel, float width) noexcept {
    const float scaledWet = wet * kWetScale;
    wetDirect = scaledWet * (width * 0.5F + 0.5F);
    wetCross = scaledWet * ((1.0F - width) * 0.5F);
    dry = dryLevel;
}

void Freeverb::process(const float* input, float* output, std::uint32_t frames, std::uint32_t channels) noexcept {
    for (std::uint32_t frame = 0; frame < frames; ++frame) {
        const float* in = input + static_cast<std::size_t>(frame) * channels;
        float* out = output + static_cast<std::size_t>(frame) * channels;
        const float left = in[0];
        const float right = channels > 1 ? in[1] : in[0];
        const float mono = (left + right) * kInputGain;

        float wetLeft = 0.0F;
        float wetRight = 0.0F;
        for (std::size_t index = 0; index < combsLeft.size(); ++index) {
            wetLeft += filter(combsLeft[index], mono);
            wetRight += filter(combsRight[index], mono);
        }
        for (std::size_t index = 0; index < allpassesLeft.size(); ++index) {
            wetLeft = diffuse(allpassesLeft[index], wetLeft);
            wetRight = diffuse(allpassesRight[index], wetRight);
        }

        out[0] = wetLeft * wetDirect + wetRight * wetCross + left * dry;
        if (channels > 1) {
            out[1] = wetRight * wetDirect + wetLeft * wetCross + right * dry;
        }
        for (std::uint32_t channel = 2; channel < channels; ++channel) {
            out[channel] = in[channel] * dry;
        }
    }
}

float Freeverb::getDecayTime(float roomSize) noexcept {
    const float longest = static_cast<float>(kCombTuning.back() + kStereoSpread) / kTuningRate;
    return longest * std::log(kSilence) / std::log(roomSize * kRoomScale + kRoomOffset);
}

std::size_t Freeverb::scaled(std::size_t samples, float ratio) noexcept {
    return std::max<std::size_t>(1, static_cast<std::size_t>(std::lround(static_cast<float>(samples) * ratio)));
}

float Freeverb::filter(Comb& comb, float input) noexcept {
    const float output = comb.buffer[comb.index];
    comb.filtered = output * (1.0F - damping) + comb.filtered * damping;
    if (std::abs(comb.filtered) < kDenormal) {
        comb.filtered = 0.0F;
    }
    comb.buffer[comb.index] = input + comb.filtered * feedback;
    if (++comb.index == comb.buffer.size()) {
        comb.index = 0;
    }
    return output;
}

float Freeverb::diffuse(Allpass& allpass, float input) noexcept {
    const float delayed = allpass.buffer[allpass.index];
    allpass.buffer[allpass.index] = input + delayed * kAllpassFeedback;
    if (++allpass.index == allpass.buffer.size()) {
        allpass.index = 0;
    }
    return delayed - input;
}

} // namespace haylen::audio
