#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "haylen/audio/Effect.hpp"

namespace haylen::audio {

// An echo that repeats the input after a time in seconds and feeds every repeat back at the feedback level, mixing the repeats at the wet level over the input at the dry level. The time changes at run time within the largest time given when the delay is created, up to 10 seconds, which sets the memory the delay holds.
class Delay final : public Effect {
  public:
    struct Settings {
        float time = 0.25F;
        float maxTime = 1.0F;
        float feedback = 0.4F;
        float wet = 0.5F;
        float dry = 1.0F;
    };

    explicit Delay(const Settings& settings);

    [[nodiscard]] float getTime() const noexcept {
        return time.load(std::memory_order_relaxed);
    }
    void setTime(float value);
    [[nodiscard]] float getMaxTime() const noexcept {
        return maxTime;
    }
    [[nodiscard]] float getFeedback() const noexcept {
        return feedback.load(std::memory_order_relaxed);
    }
    void setFeedback(float value);
    [[nodiscard]] float getWet() const noexcept {
        return wet.load(std::memory_order_relaxed);
    }
    void setWet(float value);
    [[nodiscard]] float getDry() const noexcept {
        return dry.load(std::memory_order_relaxed);
    }
    void setDry(float value);

    // Returns the time until the repeats fall 60 decibels below the first one.
    [[nodiscard]] float getTail() const noexcept override;

  private:
    static constexpr float kSilence = 0.001F;
    static constexpr float kLongestTime = 10.0F;

    void requireTime(float value) const;
    static void requireFeedback(float value);
    static void requireLevel(float value, const char* name);

    void prepare(std::uint32_t rate, std::uint32_t count) override;
    void process(const float* input, float* output, std::uint32_t frames) noexcept override;

    float maxTime;
    std::atomic<float> time;
    std::atomic<float> feedback;
    std::atomic<float> wet;
    std::atomic<float> dry;

    // Audio thread state: one interleaved ring of frames, and the delay in frames the last block ended with, which the next block glides from.
    float sampleRate = 0.0F;
    std::uint32_t channels = 0;
    std::vector<float> ring;
    std::size_t ringFrames = 0;
    std::size_t writeFrame = 0;
    double currentDelay = -1.0;
};

} // namespace haylen::audio
