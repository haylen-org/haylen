#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/audio/Sound.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/PackageReader.hpp"
#include "io/MemoryReader.hpp"
#include "support/VarnRuntime.hpp"

namespace haylen::audio {

// Builds sounds in memory and mixes them through a stereo mixer without a device, so audio tests hear exactly what the mixer renders. The voices of streamed sounds decode on the I/O pool of a job system of the fixture, whose failures a test must expect, or they fail it.
class MixerFixture : public ::testing::Test {
  protected:
    static constexpr std::uint32_t kRate = 48000;
    static constexpr std::uint64_t kPositionPeriod = 30000;
    static constexpr std::size_t kListenBlock = 512;

    // Work that no worker started goes before the job system that would report its failure.
    void TearDown() override {
        jobs.discardQueued();
        EXPECT_TRUE(jobErrors.empty()) << jobErrors.front();
    }

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

    // Reads a file from memory the way a package reads it, for the sounds that stream it.
    [[nodiscard]] static std::shared_ptr<io::PackageReader> makeReader(std::vector<std::uint8_t> file) {
        return std::make_shared<io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(std::move(file)));
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

    // Encodes the position of every frame of a stereo file, the frame within its period on the left and the period on the right, as 16-bit values from 1, so silence never reads as a position.
    [[nodiscard]] static std::vector<std::uint8_t> makePositions(std::uint64_t frames) {
        std::vector<float> samples(frames * 2);
        for (std::uint64_t frame = 0; frame < frames; ++frame) {
            samples[frame * 2] = static_cast<float>(frame % kPositionPeriod + 1) / 32767.0F;
            samples[frame * 2 + 1] = static_cast<float>(frame / kPositionPeriod + 1) / 32767.0F;
        }
        return makeWav(kRate, 2, samples);
    }

    // Mixes blocks until the voice played the count of frames or ended, polling the job system and posting the fills of streamed voices between the blocks as a frame does, and returns the position of every frame it played. Where the ring of a streamed voice ran dry the mix is silent, which holds no position.
    [[nodiscard]] std::vector<std::uint64_t> listen(Mixer::VoiceId voice, std::size_t count) {
        std::vector<std::uint64_t> positions;
        std::vector<float> block(kListenBlock * 2);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (positions.size() < count && mixer.isActive(voice) && std::chrono::steady_clock::now() < deadline) {
            runtime.getRuntime().poll();
            mixer.update(0.0F);
            mixer.render(block);
            bool silent = false;
            for (std::size_t frame = 0; frame < kListenBlock && positions.size() < count; ++frame) {
                const long left = std::lround(block[frame * 2] * 32768.0F);
                const long right = std::lround(block[frame * 2 + 1] * 32768.0F);
                if (left <= 0) {
                    silent = true;
                    continue;
                }
                positions.push_back(static_cast<std::uint64_t>(right - 1) * kPositionPeriod + static_cast<std::uint64_t>(left - 1));
            }
            if (silent) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        runtime.getRuntime().poll();
        mixer.update(0.0F);
        return positions;
    }

    // Polls the job system until the failures arrived or a deadline passed, since a job that fails ends its voice at once while its failure reaches the frame thread through the event loop.
    [[nodiscard]] bool awaitJobErrors(std::size_t count) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (jobErrors.size() < count && std::chrono::steady_clock::now() < deadline) {
            runtime.getRuntime().poll();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return jobErrors.size() >= count;
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

    test::VarnRuntime runtime;
    std::vector<std::string> jobErrors;
    core::JobSystem jobs{runtime.getRuntime(), [this](const std::string& message) { jobErrors.push_back(message); }};
    Mixer mixer{{.device = false, .sampleRate = kRate, .channels = 2, .maxVoices = 8}, jobs};
};

} // namespace haylen::audio
