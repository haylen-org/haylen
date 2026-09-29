#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

// A second-order filter from the Audio EQ Cookbook of Robert Bristow-Johnson. The cutoff is the corner frequency of the pass and shelf kinds and the center frequency of the others, q sets the resonance or the width of the band, and the gain in decibels lifts or cuts the peak and shelf kinds. A cutoff at or above half the sample rate acts just below it.
class Filter final : public Effect {
  public:
    enum class Kind : std::uint8_t {
        Lowpass,
        Highpass,
        Bandpass,
        Notch,
        Peak,
        LowShelf,
        HighShelf,
    };

    struct Settings {
        float cutoff = 1000.0F;
        float q = 0.7071F;
        float gain = 0.0F;
    };

    Filter(Kind kind, const Settings& settings);

    [[nodiscard]] Kind getKind() const noexcept {
        return kind;
    }
    [[nodiscard]] float getCutoff() const noexcept {
        return cutoff.load(std::memory_order_relaxed);
    }
    void setCutoff(float value);
    [[nodiscard]] float getQ() const noexcept {
        return q.load(std::memory_order_relaxed);
    }
    void setQ(float value);
    [[nodiscard]] float getGain() const noexcept {
        return gain.load(std::memory_order_relaxed);
    }

    // Only the peak and shelf kinds have a gain, from -96 to 96 decibels, a range whose coefficients always fit the filter.
    void setGain(float value);
    [[nodiscard]] bool hasGain() const noexcept;

    [[nodiscard]] float getTail() const noexcept override {
        return 0.0F;
    }

  private:
    // Biquad coefficients normalized by a0, for the transposed direct form II.
    struct Coefficients {
        float b0 = 1.0F;
        float b1 = 0.0F;
        float b2 = 0.0F;
        float a1 = 0.0F;
        float a2 = 0.0F;
    };

    static constexpr float kMaxGain = 96.0F;

    [[nodiscard]] static Coefficients design(Kind type, float rate, float frequency, float quality, float decibels) noexcept;
    static void requireCutoff(float value);
    static void requireQ(float value);
    static void requireGain(float value);

    void prepare(std::uint32_t rate, std::uint32_t count) override;
    void process(const float* input, float* output, std::uint32_t frames) noexcept override;
    void changed() noexcept;

    Kind kind;
    std::atomic<float> cutoff;
    std::atomic<float> q;
    std::atomic<float> gain;
    std::atomic<std::uint32_t> version{1};

    // Audio thread state, with two delay elements per channel.
    std::uint32_t appliedVersion = 0;
    float sampleRate = 0.0F;
    std::uint32_t channels = 0;
    Coefficients coefficients;
    std::vector<float> history;
};

} // namespace haylen::audio
