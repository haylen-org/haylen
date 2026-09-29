#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"

namespace haylen::audio {

// Builds sounds in memory and mixes them through a stereo mixer without a device, so audio tests hear exactly what the mixer renders.
class MixerFixture : public ::testing::Test {
  protected:
    static constexpr std::uint32_t kRate = 48000;

    // Encodes interleaved samples in the range -1 to 1 as a 16-bit PCM WAV file.
    [[nodiscard]] static std::vector<std::uint8_t> makeWav(std::uint32_t sampleRate, std::uint16_t channels, const std::vector<float>& samples) {
        std::vector<std::uint8_t> file;
        // clang-format off
        const auto append = [&file](std::uint32_t value, int bytes) {
            for (int index = 0; index < bytes; ++index) {
                file.push_back(static_cast<std::uint8_t>((value >> (8 * index)) & 0xFFU));
            }
        };
        // clang-format on
        const auto appendText = [&file](const char* text) { file.insert(file.end(), text, text + 4); };
        const auto dataSize = static_cast<std::uint32_t>(samples.size() * 2);

        appendText("RIFF");
        append(36 + dataSize, 4);
        appendText("WAVE");
        appendText("fmt ");
        append(16, 4);
        append(1, 2);
        append(channels, 2);
        append(sampleRate, 4);
        append(sampleRate * channels * 2, 4);
        append(channels * 2U, 2);
        append(16, 2);
        appendText("data");
        append(dataSize, 4);
        for (const float sample : samples) {
            const auto value = static_cast<std::int16_t>(std::lround(std::clamp(sample, -1.0F, 1.0F) * 32767.0F));
            append(static_cast<std::uint16_t>(value), 2);
        }
        return file;
    }

    // Returns interleaved frames that hold the same value on every channel.
    [[nodiscard]] static std::vector<float> makeSignal(std::size_t frames, std::uint16_t channels, float value) {
        return std::vector<float>(frames * channels, value);
    }

    [[nodiscard]] static Sound makeTone(std::size_t frames, float value = 0.5F) {
        return Sound::decode(makeWav(kRate, 1, makeSignal(frames, 1, value)));
    }

    // A whole number of periods, so the sine loops without a seam.
    [[nodiscard]] static Sound makeSine(float frequency, float amplitude = 0.5F) {
        const auto period = static_cast<std::size_t>(std::lround(static_cast<float>(kRate) / frequency));
        std::vector<float> samples(period * static_cast<std::size_t>(std::max(1.0F, frequency / 10.0F)));
        for (std::size_t index = 0; index < samples.size(); ++index) {
            samples[index] = amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * static_cast<double>(index) / static_cast<double>(period)));
        }
        return Sound::decode(makeWav(kRate, 1, samples));
    }

    // One sample at the value followed by silence.
    [[nodiscard]] static Sound makeImpulse(std::size_t frames, float value = 1.0F) {
        std::vector<float> samples(frames, 0.0F);
        samples.front() = value;
        return Sound::decode(makeWav(kRate, 1, samples));
    }

    [[nodiscard]] static std::size_t toFrames(float seconds) {
        return static_cast<std::size_t>(seconds * static_cast<float>(kRate));
    }

    // Mixes the frames and returns the left channel.
    [[nodiscard]] std::vector<float> renderLeft(std::size_t frames) {
        std::vector<float> samples(frames * mixer.getChannels());
        mixer.render(samples);
        std::vector<float> left(frames);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            left[frame] = samples[frame * mixer.getChannels()];
        }
        return left;
    }

    // Mixes the given number of frames and returns the loudest sample.
    float peak(std::size_t count) {
        std::vector<float> samples(count * mixer.getChannels());
        mixer.render(samples);
        float loudest = 0.0F;
        for (const float sample : samples) {
            loudest = std::max(loudest, std::abs(sample));
        }
        return loudest;
    }

    // The mixer renders ahead in small periods, so changes show up in the output one period later.
    float settledPeak(std::size_t count) {
        peak(2048);
        return peak(count);
    }

    Mixer mixer{{.device = false, .sampleRate = kRate, .channels = 2, .maxVoices = 8}};
};

} // namespace haylen::audio
