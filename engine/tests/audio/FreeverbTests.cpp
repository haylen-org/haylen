#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "audio/Freeverb.hpp"

namespace haylen::audio {

class FreeverbTest : public ::testing::Test {
  protected:
    static constexpr std::uint32_t kRate = 48000;

    // Runs an impulse on both channels through the reverb and returns the interleaved response.
    [[nodiscard]] static std::vector<float> respond(Freeverb& reverb, float seconds, std::uint32_t channels = 2) {
        const auto frames = static_cast<std::uint32_t>(seconds * static_cast<float>(kRate));
        std::vector<float> input(static_cast<std::size_t>(frames) * channels, 0.0F);
        std::fill_n(input.begin(), channels, 1.0F);
        std::vector<float> output(input.size());
        reverb.process(input.data(), output.data(), frames, channels);
        return output;
    }

    [[nodiscard]] static double energy(const std::vector<float>& samples, float from, float to) {
        const auto first = static_cast<std::size_t>(from * static_cast<float>(kRate)) * 2;
        const auto last = static_cast<std::size_t>(to * static_cast<float>(kRate)) * 2;
        double sum = 0.0;
        for (std::size_t index = first; index < last; ++index) {
            sum += static_cast<double>(samples[index]) * samples[index];
        }

        return sum;
    }

    // Returns the first frame where a channel of an interleaved stereo response sounds, or the number of frames when it stays silent.
    [[nodiscard]] static std::size_t firstSound(const std::vector<float>& response, std::size_t channel) {
        const std::size_t frames = response.size() / 2;
        for (std::size_t frame = 0; frame < frames; ++frame) {
            if (response[frame * 2 + channel] != 0.0F) {
                return frame;
            }
        }

        return frames;
    }
};

TEST_F(FreeverbTest, ImpulseResponseArrivesLateAndDecays) {
    Freeverb reverb(kRate);
    reverb.setRoomSize(0.5F);
    reverb.setDamping(0.5F);
    reverb.setMix(1.0F, 0.0F, 1.0F);
    const std::vector<float> response = respond(reverb, 4.0F);
    EXPECT_TRUE(std::ranges::all_of(response, [](float sample) { return std::isfinite(sample); }));

    // Nothing comes out before the shortest comb filter, which is 1116 samples at 44100 hertz, and the right side comes 23 samples later.
    const std::size_t firstLeft = firstSound(response, 0);
    const std::size_t firstRight = firstSound(response, 1);
    EXPECT_EQ(firstLeft, 1215U);
    EXPECT_EQ(firstRight, 1240U);

    const double early = energy(response, 0.0F, 0.2F);
    const double middle = energy(response, 1.0F, 1.2F);
    const double late = energy(response, 3.5F, 3.7F);
    EXPECT_GT(early, 0.0);
    EXPECT_LT(middle, early * 0.01);
    EXPECT_LT(late, early * 1e-9);

    double difference = 0.0;
    for (std::size_t frame = 0; frame < response.size() / 2; ++frame) {
        difference += std::abs(response[frame * 2] - response[frame * 2 + 1]);
    }

    EXPECT_GT(difference, 0.1);
}

TEST_F(FreeverbTest, RoomSizeSetsTheDecay) {
    Freeverb small(kRate);
    small.setRoomSize(0.3F);
    small.setMix(1.0F, 0.0F, 1.0F);
    Freeverb large(kRate);
    large.setRoomSize(0.9F);
    large.setMix(1.0F, 0.0F, 1.0F);
    EXPECT_GT(energy(respond(large, 1.2F), 1.0F, 1.2F), energy(respond(small, 1.2F), 1.0F, 1.2F) * 100.0);

    EXPECT_NEAR(Freeverb::getDecayTime(0.5F), 1.473F, 0.01F);
    EXPECT_GT(Freeverb::getDecayTime(1.0F), Freeverb::getDecayTime(0.5F));
}

TEST_F(FreeverbTest, DryLevelPassesEveryChannel) {
    Freeverb reverb(22050);
    reverb.setMix(0.0F, 0.5F, 1.0F);
    const std::vector<float> input{1.0F, -1.0F, 0.5F, 0.25F, 0.0F, 1.0F};
    std::vector<float> output(input.size());
    reverb.process(input.data(), output.data(), 2, 3);
    EXPECT_EQ(output, (std::vector<float>{0.5F, -0.5F, 0.25F, 0.125F, 0.0F, 0.5F}));

    std::vector<float> mono(4, 0.0F);
    reverb.process(input.data(), mono.data(), 4, 1);
    EXPECT_EQ(mono, (std::vector<float>{0.5F, -0.5F, 0.25F, 0.125F}));
}

} // namespace haylen::audio
