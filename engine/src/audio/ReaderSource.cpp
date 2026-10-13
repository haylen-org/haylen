#include "audio/ReaderSource.hpp"

#include <algorithm>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

#include "audio/ReaderDecoder.hpp"
#include "audio/SoundData.hpp"

namespace haylen::audio {

// What a voice shares with its fills, which may outlive it. A fill writes frames at `written` and the audio thread takes them at `taken`, so each side moves only its own count. A seek of the audio thread raises `generation`, and from then on the audio thread takes nothing until a fill dropped the frames it never took, moved the decoder and set `ready` to the generation it served.
struct ReaderSource::Feed {
    explicit Feed(const SoundData& sound) : reader(sound.reader), channels(sound.channels), sampleRate(sound.sampleRate), length(sound.frames), capacity(std::max<std::size_t>(1, static_cast<std::size_t>(sound.sampleRate * kBufferSeconds))), ring(capacity * channels), loopEnd(sound.frames > 0 ? sound.frames : std::numeric_limits<std::uint64_t>::max()) {}

    // Runs on the I/O pool, one fill at a time.
    void fill() {
        const std::uint64_t served = generation.load(std::memory_order_acquire);
        try {
            decode(served);
        } catch (...) {
            failed.store(true, std::memory_order_release);
            filling.store(false, std::memory_order_release);
            throw;
        }
        filling.store(false, std::memory_order_release);
    }

    void decode(std::uint64_t served) {
        if (!decoder) {
            decoder = std::make_unique<ReaderDecoder>(reader);
        }
        if (ready.load(std::memory_order_relaxed) != served) {
            taken.store(written.load(std::memory_order_relaxed), std::memory_order_relaxed);
            position = target.load(std::memory_order_relaxed);
            finished.store(!decoder->seek(position), std::memory_order_relaxed);
            ready.store(served, std::memory_order_release);
        }

        // The fill stops early when the voice seeks again, since the next fill serves the new cursor.
        bool wrapped = false;
        while (!finished.load(std::memory_order_relaxed) && generation.load(std::memory_order_relaxed) == served) {
            const std::uint64_t head = written.load(std::memory_order_relaxed);
            const std::size_t room = capacity - static_cast<std::size_t>(head - taken.load(std::memory_order_acquire));
            if (room == 0) {
                return;
            }
            // A looping voice decodes up to the end of its loop and goes on from its start.
            const bool loops = looping.load(std::memory_order_relaxed);
            const std::uint64_t end = loopEnd.load(std::memory_order_relaxed);
            if (loops && position >= end) {
                if (!restartLoop()) {
                    return;
                }
                continue;
            }
            const std::size_t offset = ringOffset(head);
            const std::size_t count = loops ? static_cast<std::size_t>(std::min<std::uint64_t>(std::min(room, capacity - offset), end - position)) : std::min(room, capacity - offset);
            const std::uint64_t decoded = decoder->read(std::span(ring).subspan(offset * channels, count * channels));
            written.store(head + decoded, std::memory_order_release);
            position += decoded;
            if (decoded > 0) {
                wrapped = false;
            }
            if (decoded == count) {
                continue;
            }

            // The file ended before the end of the loop. A looping voice goes on from the start of its loop, unless the loop has no frames.
            if (!loops || wrapped || !restartLoop()) {
                finished.store(true, std::memory_order_release);
                return;
            }
            wrapped = true;
        }
    }

    bool restartLoop() {
        position = loopBegin.load(std::memory_order_relaxed);
        if (!decoder->seek(position)) {
            finished.store(true, std::memory_order_release);
            return false;
        }
        return true;
    }

    // Copies the frames that arrived, at most the count, and returns how many.
    std::size_t take(float* frames, std::size_t count) {
        const std::uint64_t start = taken.load(std::memory_order_relaxed);
        const std::size_t total = std::min(count, static_cast<std::size_t>(written.load(std::memory_order_acquire) - start));
        for (std::size_t copied = 0; copied < total;) {
            const std::size_t offset = ringOffset(start + copied);
            const std::size_t run = std::min(total - copied, capacity - offset);
            std::copy_n(ring.begin() + static_cast<std::ptrdiff_t>(offset * channels), run * channels, frames + copied * channels);
            copied += run;
        }
        taken.store(start + total, std::memory_order_release);
        return total;
    }

    // The place in the ring of a frame of the stream. The counts of the stream take 64 bits, while the ring always fits in memory.
    [[nodiscard]] std::size_t ringOffset(std::uint64_t frame) const noexcept {
        return static_cast<std::size_t>(frame % capacity);
    }

    std::shared_ptr<io::PackageReader> reader;
    std::unique_ptr<ReaderDecoder> decoder;
    std::uint32_t channels;
    std::uint32_t sampleRate;
    std::uint64_t length;
    std::size_t capacity;
    std::vector<float> ring;
    std::atomic<std::uint64_t> written{0};
    std::atomic<std::uint64_t> taken{0};
    std::atomic<std::uint64_t> generation{0};
    std::atomic<std::uint64_t> ready{0};
    std::atomic<std::uint64_t> target{0};
    std::atomic<bool> finished{false};
    std::atomic<bool> failed{false};
    std::atomic<bool> looping{false};
    std::atomic<bool> filling{false};
    std::atomic<std::uint64_t> loopBegin{0};
    std::atomic<std::uint64_t> loopEnd;
    std::uint64_t position = 0;
};

const ma_data_source_vtable ReaderSource::kVtable{&ReaderSource::read, &ReaderSource::seek, &ReaderSource::getFormat, &ReaderSource::getCursor, &ReaderSource::getLength, &ReaderSource::setLooping, MA_DATA_SOURCE_SELF_MANAGED_RANGE_AND_LOOP_POINT};

ReaderSource::ReaderSource(const SoundData& sound) : feed(std::make_shared<Feed>(sound)) {
    ma_data_source_config config = ma_data_source_config_init();
    config.vtable = &kVtable;
    if (ma_data_source_init(&config, get()) != MA_SUCCESS) {
        throw std::runtime_error("A streamed sound could not be prepared for playback.");
    }
}

ReaderSource::~ReaderSource() {
    ma_data_source_uninit(get());
}

void ReaderSource::setLoopRegion(std::uint64_t begin, std::uint64_t end) noexcept {
    feed->loopBegin.store(begin, std::memory_order_relaxed);
    feed->loopEnd.store(end, std::memory_order_relaxed);
}

std::function<void()> ReaderSource::takeFill() {
    Feed& state = *feed;
    if (state.failed.load(std::memory_order_acquire)) {
        return {};
    }

    // The counts move while they are read, so the free room is only an estimate, which the next frame corrects.
    const bool moved = state.ready.load(std::memory_order_acquire) != state.generation.load(std::memory_order_acquire);
    const std::uint64_t taken = state.taken.load(std::memory_order_acquire);
    const std::uint64_t buffered = std::min<std::uint64_t>(state.capacity, state.written.load(std::memory_order_acquire) - taken);
    const bool room = !state.finished.load(std::memory_order_acquire) && state.capacity - buffered >= state.capacity / kFillDivisor;
    if ((!moved && !room) || state.filling.exchange(true, std::memory_order_acq_rel)) {
        return {};
    }
    return [shared = feed] { shared->fill(); };
}

// A read takes the frames that arrived and plays silence for the rest, so the voice goes on while a fill runs late, and only a stream whose fill reached the end or failed ends the voice.
ma_result ReaderSource::read(ma_data_source* source, void* frames, ma_uint64 frameCount, ma_uint64* framesRead) {
    auto& self = *static_cast<ReaderSource*>(source);
    Feed& state = *self.feed;
    auto* output = static_cast<float*>(frames);
    const auto count = static_cast<std::size_t>(frameCount);
    *framesRead = 0;
    if (state.failed.load(std::memory_order_acquire)) {
        return MA_AT_END;
    }

    std::size_t delivered = 0;
    if (state.ready.load(std::memory_order_acquire) == state.generation.load(std::memory_order_relaxed)) {
        delivered = state.take(output, count);
        ma_uint64 moved = self.cursor.load(std::memory_order_relaxed) + delivered;
        const ma_uint64 begin = state.loopBegin.load(std::memory_order_relaxed);
        const ma_uint64 end = state.loopEnd.load(std::memory_order_relaxed);
        if (state.looping.load(std::memory_order_relaxed) && end > begin && moved >= end) {
            moved = begin + (moved - begin) % (end - begin);
        }
        self.cursor.store(moved, std::memory_order_relaxed);
        if (delivered < count && state.finished.load(std::memory_order_acquire) && state.written.load(std::memory_order_acquire) == state.taken.load(std::memory_order_relaxed)) {
            *framesRead = delivered;
            return delivered == 0 ? MA_AT_END : MA_SUCCESS;
        }
    }

    std::fill(output + delivered * state.channels, output + count * state.channels, 0.0F);
    *framesRead = frameCount;
    return MA_SUCCESS;
}

ma_result ReaderSource::seek(ma_data_source* source, ma_uint64 frame) {
    auto& self = *static_cast<ReaderSource*>(source);
    self.cursor.store(frame, std::memory_order_relaxed);
    self.feed->target.store(frame, std::memory_order_relaxed);
    self.feed->generation.fetch_add(1, std::memory_order_release);
    return MA_SUCCESS;
}

ma_result ReaderSource::getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, std::size_t channelMapCapacity) {
    const Feed& state = *static_cast<ReaderSource*>(source)->feed;
    *format = ma_format_f32;
    *channels = state.channels;
    *sampleRate = state.sampleRate;
    if (channelMap != nullptr) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCapacity, state.channels);
    }
    return MA_SUCCESS;
}

ma_result ReaderSource::getCursor(ma_data_source* source, ma_uint64* cursor) {
    *cursor = static_cast<ReaderSource*>(source)->cursor.load(std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result ReaderSource::getLength(ma_data_source* source, ma_uint64* length) {
    *length = static_cast<ReaderSource*>(source)->feed->length;
    return *length > 0 ? MA_SUCCESS : MA_NOT_IMPLEMENTED;
}

ma_result ReaderSource::setLooping(ma_data_source* source, ma_bool32 looping) {
    static_cast<ReaderSource*>(source)->feed->looping.store(looping == MA_TRUE, std::memory_order_relaxed);
    return MA_SUCCESS;
}

} // namespace haylen::audio
