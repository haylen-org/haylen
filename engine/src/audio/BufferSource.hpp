#pragma once

#include <miniaudio.h>

#include <atomic>
#include <cstddef>

namespace haylen::audio {

struct SoundData;

// The miniaudio data source through which a voice reads the samples of a decoded sound, which every voice of the sound shares. The mixing thread moves the cursor of the voice while the frame thread reads it, so the cursor is atomic.
class BufferSource final {
  public:
    explicit BufferSource(const SoundData& value);
    ~BufferSource();

    BufferSource(const BufferSource&) = delete;
    BufferSource& operator=(const BufferSource&) = delete;

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
    const SoundData* sound;
    std::atomic<ma_uint64> cursor{0};
};

} // namespace haylen::audio
