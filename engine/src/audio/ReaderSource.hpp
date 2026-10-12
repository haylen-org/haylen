#pragma once

#include <miniaudio.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>

namespace haylen::audio {

struct SoundData;

// The miniaudio data source through which a voice plays a streamed sound, as floats in the rate and channels of the file, which the voice converts to the mixer. Fills on the I/O pool decode the file through its package reader into a ring of frames ahead of the voice, and the audio thread only takes frames from the ring, so it never reads the package or waits: it plays silence where the frames have not arrived, and a seek only tells the next fill where to decode from. A looping voice loops without a gap, because the fill decodes the start of the file again right after its end.
class ReaderSource final {
  public:
    // The seconds of frames the ring holds, which cover a frame thread that stalls for that long, and the share of them that must be free before the next fill starts.
    static constexpr double kBufferSeconds = 1.0;
    static constexpr std::uint64_t kFillDivisor = 4;

    explicit ReaderSource(const SoundData& sound);
    ~ReaderSource();

    ReaderSource(const ReaderSource&) = delete;
    ReaderSource& operator=(const ReaderSource&) = delete;

    [[nodiscard]] ma_data_source* get() noexcept {
        return &base;
    }

    // Returns the job that tops the ring up on the I/O pool, while the voice has room in its ring or moved its cursor, and an empty function while a fill runs or nothing needs one. A fill that fails ends the voice and throws its failure. The frame thread calls it once per frame.
    [[nodiscard]] std::function<void()> takeFill();

  private:
    struct Feed;

    static const ma_data_source_vtable kVtable;

    static ma_result read(ma_data_source* source, void* frames, ma_uint64 frameCount, ma_uint64* framesRead);
    static ma_result seek(ma_data_source* source, ma_uint64 frame);
    static ma_result getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, std::size_t channelMapCapacity);
    static ma_result getCursor(ma_data_source* source, ma_uint64* cursor);
    static ma_result getLength(ma_data_source* source, ma_uint64* length);
    static ma_result setLooping(ma_data_source* source, ma_bool32 looping);

    // The base stays the first member of this standard-layout class, because miniaudio reaches the source through it.
    ma_data_source_base base{};
    std::shared_ptr<Feed> feed;

    // The audio thread moves the cursor of the voice and the frame thread reads it.
    std::atomic<ma_uint64> cursor{0};
};

} // namespace haylen::audio
