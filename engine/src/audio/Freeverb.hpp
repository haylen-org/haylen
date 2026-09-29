#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace haylen::audio {

// The Freeverb reverb of Jezar at Dreampoint: eight parallel comb filters with damped feedback and four allpass filters in series for each side, the right side spread a few samples apart. It sums the first two channels of interleaved frames into the reverb and adds the reverb to the first two channels, while the other channels keep only the dry level.
class Freeverb final {
  public:
    explicit Freeverb(std::uint32_t sampleRate);

    void setRoomSize(float value) noexcept;
    void setDamping(float value) noexcept;
    void setMix(float wet, float dryLevel, float width) noexcept;
    void process(const float* input, float* output, std::uint32_t frames, std::uint32_t channels) noexcept;

    // Returns the seconds the longest comb filter takes to fall 60 decibels with the feedback of the room size, which the tuning keeps the same at every sample rate.
    [[nodiscard]] static float getDecayTime(float roomSize) noexcept;

  private:
    struct Comb {
        std::vector<float> buffer;
        std::size_t index = 0;
        float filtered = 0.0F;
    };

    struct Allpass {
        std::vector<float> buffer;
        std::size_t index = 0;
    };

    // Delay lengths in samples at 44100 hertz.
    static constexpr std::array<std::size_t, 8> kCombTuning{1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617};
    static constexpr std::array<std::size_t, 4> kAllpassTuning{556, 441, 341, 225};
    static constexpr std::size_t kStereoSpread = 23;
    static constexpr float kTuningRate = 44100.0F;
    static constexpr float kInputGain = 0.015F;
    static constexpr float kWetScale = 3.0F;
    static constexpr float kDampingScale = 0.4F;
    static constexpr float kRoomScale = 0.28F;
    static constexpr float kRoomOffset = 0.7F;
    static constexpr float kAllpassFeedback = 0.5F;
    static constexpr float kSilence = 0.001F;

    // Comb memory settles to exact zero instead of lingering in denormal numbers, which are slow on many processors.
    static constexpr float kDenormal = 1.0e-20F;

    [[nodiscard]] static std::size_t scaled(std::size_t samples, float ratio) noexcept;
    [[nodiscard]] float filter(Comb& comb, float input) noexcept;
    [[nodiscard]] static float diffuse(Allpass& allpass, float input) noexcept;

    std::array<Comb, 8> combsLeft;
    std::array<Comb, 8> combsRight;
    std::array<Allpass, 4> allpassesLeft;
    std::array<Allpass, 4> allpassesRight;
    float feedback = 0.0F;
    float damping = 0.0F;
    float wetDirect = 0.0F;
    float wetCross = 0.0F;
    float dry = 1.0F;
};

} // namespace haylen::audio
