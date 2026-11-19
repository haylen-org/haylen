#include "haylen/audio/Sound.hpp"

#include <miniaudio.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "audio/MemoryDecoder.hpp"
#include "audio/ReaderDecoder.hpp"
#include "audio/SoundData.hpp"

namespace haylen::audio {

Sound Sound::decode(std::span<const std::uint8_t> encoded) {
    MemoryDecoder decoder(encoded);
    SoundData::Content decoded{.channels = decoder.get().outputChannels, .sampleRate = decoder.get().outputSampleRate};

    // Some formats only know their length after a full scan, so frames are read in chunks until the decoder runs dry.
    std::array<float, 4096> chunk{};
    const ma_uint64 framesPerChunk = chunk.size() / decoded.channels;
    for (;;) {
        ma_uint64 read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder.get(), chunk.data(), framesPerChunk, &read);
        if (decoded.samples.size() + read * decoded.channels > kMaxSamples) {
            throw std::runtime_error("The audio decodes to more than " + std::to_string(kMaxSamples) + " samples, so it plays only as a stream.");
        }
        decoded.samples.insert(decoded.samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read * decoded.channels));

        if (result != MA_SUCCESS || read < framesPerChunk) {
            break;
        }
    }

    decoded.frames = decoded.samples.size() / decoded.channels;
    if (decoded.frames == 0) {
        throw std::runtime_error("Audio data contains no samples.");
    }

    return Sound(std::make_shared<SoundData>(std::move(decoded)));
}

Sound Sound::fromSamples(std::span<const float> samples, std::uint32_t channels, std::uint32_t sampleRate) {
    if (channels == 0 || sampleRate == 0) {
        throw std::invalid_argument("Raw audio needs a sample rate and channels.");
    }

    if (samples.size() % channels != 0) {
        throw std::invalid_argument("Raw audio samples come in whole frames, one sample for every channel.");
    }

    if (samples.empty()) {
        throw std::runtime_error("Audio data contains no samples.");
    }

    SoundData::Content decoded{.samples = std::vector<float>(samples.begin(), samples.end()), .channels = channels, .sampleRate = sampleRate, .frames = samples.size() / channels};
    return Sound(std::make_shared<SoundData>(std::move(decoded)));
}

Sound Sound::fromSamples(std::span<const std::int16_t> samples, std::uint32_t channels, std::uint32_t sampleRate) {
    std::vector<float> converted(samples.size());
    std::ranges::transform(samples, converted.begin(), [](std::int16_t sample) { return static_cast<float>(sample) / 32768.0F; });
    return fromSamples(converted, channels, sampleRate);
}

Sound Sound::stream(std::shared_ptr<io::PackageReader> reader) {
    if (!reader) {
        throw std::invalid_argument("A streamed sound needs a reader of its file.");
    }

    SoundData::Content streamed;
    {
        ReaderDecoder decoder(reader);
        streamed.channels = decoder.getChannels();
        streamed.sampleRate = decoder.getSampleRate();

        // Every voice of the sound keeps a second of its frames ahead, so the header of the file bounds what a voice allocates.
        const auto maxSampleRate = static_cast<std::uint32_t>(ma_standard_sample_rate_max);
        const auto maxChannels = static_cast<std::uint32_t>(MA_MAX_CHANNELS);
        if (streamed.sampleRate > maxSampleRate || streamed.channels > maxChannels) {
            throw std::runtime_error("The audio file declares a sample rate above " + std::to_string(maxSampleRate) + " or more than " + std::to_string(maxChannels) + " channels, which a streamed sound cannot play.");
        }

        streamed.frames = decoder.getLength();
    }
    streamed.reader = std::move(reader);
    return Sound(std::make_shared<SoundData>(std::move(streamed)));
}

bool Sound::isStreamed() const {
    return data != nullptr && data->getContent()->reader != nullptr;
}

std::uint32_t Sound::getChannels() const {
    return data != nullptr ? data->getContent()->channels : 0;
}

std::uint32_t Sound::getSampleRate() const {
    return data != nullptr ? data->getContent()->sampleRate : 0;
}

std::uint64_t Sound::getFrameCount() const {
    return data != nullptr ? data->getContent()->frames : 0;
}

float Sound::getDuration() const {
    if (data == nullptr) {
        return 0.0F;
    }

    const std::shared_ptr<const SoundData::Content> content = data->getContent();
    return content->sampleRate > 0 ? static_cast<float>(static_cast<double>(content->frames) / content->sampleRate) : 0.0F;
}

} // namespace haylen::audio
