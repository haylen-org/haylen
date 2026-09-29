#include "haylen/audio/Sound.hpp"

#include <miniaudio.h>

#include <array>
#include <cstddef>
#include <stdexcept>

#include "audio/MemoryDecoder.hpp"
#include "audio/SoundData.hpp"

namespace haylen::audio {

debug::ObjectCounter SoundData::counter("Sound", debug::ObjectCounter::Kind::Native);

Sound Sound::decode(std::span<const std::uint8_t> encoded) {
    MemoryDecoder decoder(encoded);
    auto decoded = std::make_shared<SoundData>();
    decoded->channels = decoder.get().outputChannels;
    decoded->sampleRate = decoder.get().outputSampleRate;

    // Some formats only know their length after a full scan, so frames are read in chunks until the decoder runs dry.
    std::array<float, 4096> chunk{};
    const ma_uint64 framesPerChunk = chunk.size() / decoded->channels;
    for (;;) {
        ma_uint64 read = 0;
        const ma_result result = ma_decoder_read_pcm_frames(&decoder.get(), chunk.data(), framesPerChunk, &read);
        decoded->samples.insert(decoded->samples.end(), chunk.begin(), chunk.begin() + static_cast<std::ptrdiff_t>(read * decoded->channels));
        if (result != MA_SUCCESS || read < framesPerChunk) {
            break;
        }
    }

    decoded->frames = decoded->samples.size() / decoded->channels;
    if (decoded->frames == 0) {
        throw std::runtime_error("Audio data contains no samples.");
    }
    decoded->tracked.setBytes(decoded->samples.size() * sizeof(float));
    return Sound(std::move(decoded));
}

Sound Sound::stream(std::vector<std::uint8_t> encoded) {
    auto streamed = std::make_shared<SoundData>();
    {
        MemoryDecoder decoder(encoded);
        streamed->channels = decoder.get().outputChannels;
        streamed->sampleRate = decoder.get().outputSampleRate;
        ma_uint64 frames = 0;
        ma_decoder_get_length_in_pcm_frames(&decoder.get(), &frames);
        streamed->frames = frames;
    }
    streamed->encoded = std::move(encoded);
    streamed->streamed = true;
    streamed->tracked.setBytes(streamed->encoded.size());
    return Sound(std::move(streamed));
}

bool Sound::isStreamed() const noexcept {
    return data != nullptr && data->streamed;
}

std::uint32_t Sound::getChannels() const noexcept {
    return data != nullptr ? data->channels : 0;
}

std::uint32_t Sound::getSampleRate() const noexcept {
    return data != nullptr ? data->sampleRate : 0;
}

std::uint64_t Sound::getFrameCount() const noexcept {
    return data != nullptr ? data->frames : 0;
}

float Sound::getDuration() const noexcept {
    return data != nullptr && data->sampleRate > 0 ? static_cast<float>(static_cast<double>(data->frames) / data->sampleRate) : 0.0F;
}

} // namespace haylen::audio
