#pragma once

#include <miniaudio.h>

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace haylen::platform {
class AudioStream;
}

namespace haylen::audio {

// The miniaudio data source through which a voice reads an audio stream, as floats in the rate and channels of the stream, which the voice converts to the mixer. It takes the stream over from the voice that read it before, never ends, and reads silence where samples have not arrived.
class StreamSource final {
  public:
    explicit StreamSource(platform::AudioStream& value);
    ~StreamSource();

    StreamSource(const StreamSource&) = delete;
    StreamSource& operator=(const StreamSource&) = delete;

    [[nodiscard]] ma_data_source* get() noexcept {
        return &base;
    }

  private:
    static const ma_data_source_vtable kVtable;

    static ma_result read(ma_data_source* source, void* frames, ma_uint64 frameCount, ma_uint64* framesRead);
    static ma_result seek(ma_data_source* source, ma_uint64 frame);
    static ma_result getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, std::size_t channelMapCapacity);
    static ma_result getCursor(ma_data_source* source, ma_uint64* cursor);
    static ma_result getLength(ma_data_source* source, ma_uint64* length);

    // The base stays the first member of this standard-layout class, because miniaudio reaches the source through it.
    ma_data_source_base base{};
    platform::AudioStream* stream;
    std::uint64_t token;

    // The mixing thread counts the frames it read and the frame thread reads the count as the cursor of the voice.
    std::atomic<ma_uint64> framesRead{0};
};

} // namespace haylen::audio
