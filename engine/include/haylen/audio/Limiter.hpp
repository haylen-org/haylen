#pragma once

#include <atomic>
#include <cstdint>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

// Keeps every sample under the threshold, so many loud sounds at once never clip and crackle. The gain drops at once to fit a peak and comes back toward 1 over the release time in seconds, and the threshold is a level from above 0 to 1.
class Limiter final : public Effect {
  public:
    struct Settings {
        float threshold = 0.9F;
        float release = 0.1F;
    };

    explicit Limiter(const Settings& settings);

    [[nodiscard]] float getThreshold() const noexcept {
        return threshold.load(std::memory_order_relaxed);
    }
    void setThreshold(float value);
    [[nodiscard]] float getRelease() const noexcept {
        return release.load(std::memory_order_relaxed);
    }
    void setRelease(float value);

    // Returns how far the gain dropped in the last block it processed, from 0 to 1, for meters.
    [[nodiscard]] float getReduction() const noexcept {
        return 1.0F - gainOut.load(std::memory_order_relaxed);
    }

    [[nodiscard]] float getTail() const noexcept override {
        return 0.0F;
    }

  private:
    static void requireThreshold(float value);
    static void requireRelease(float value);

    void prepare(std::uint32_t rate, std::uint32_t count) override;
    void process(const float* input, float* output, std::uint32_t frames) noexcept override;

    std::atomic<float> threshold;
    std::atomic<float> release;
    std::atomic<float> gainOut{1.0F};

    // Audio thread state: the gain the last frame ended with.
    float sampleRate = 0.0F;
    std::uint32_t channels = 0;
    float gain = 1.0F;
};

} // namespace haylen::audio
