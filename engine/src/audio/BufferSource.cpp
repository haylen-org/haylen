#include "audio/BufferSource.hpp"

#include <algorithm>
#include <stdexcept>

#include "audio/SoundData.hpp"

namespace haylen::audio {

const ma_data_source_vtable BufferSource::kVtable{&BufferSource::read, &BufferSource::seek, &BufferSource::getFormat, &BufferSource::getCursor, &BufferSource::getLength, nullptr, 0};

BufferSource::BufferSource(const SoundData& value) : sound(&value) {
    ma_data_source_config config = ma_data_source_config_init();
    config.vtable = &kVtable;
    if (ma_data_source_init(&config, get()) != MA_SUCCESS) {
        throw std::runtime_error("A sound could not be prepared for playback.");
    }
}

BufferSource::~BufferSource() {
    ma_data_source_uninit(get());
}

// A read that reaches the end of the sound comes up short, which the base of the source answers by looping or by ending the voice.
ma_result BufferSource::read(ma_data_source* source, void* frames, ma_uint64 frameCount, ma_uint64* framesRead) {
    auto& self = *static_cast<BufferSource*>(source);
    const SoundData& data = *self.sound;
    const ma_uint64 start = self.cursor.load(std::memory_order_relaxed);
    const ma_uint64 count = std::min(frameCount, data.frames - start);
    const auto first = data.samples.begin() + static_cast<std::ptrdiff_t>(start * data.channels);
    std::copy_n(first, static_cast<std::size_t>(count * data.channels), static_cast<float*>(frames));
    self.cursor.store(start + count, std::memory_order_relaxed);

    if (framesRead != nullptr) {
        *framesRead = count;
    }

    return count < frameCount ? MA_AT_END : MA_SUCCESS;
}

ma_result BufferSource::seek(ma_data_source* source, ma_uint64 frame) {
    auto& self = *static_cast<BufferSource*>(source);
    if (frame > self.sound->frames) {
        return MA_INVALID_ARGS;
    }

    self.cursor.store(frame, std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result BufferSource::getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, std::size_t channelMapCapacity) {
    const SoundData& data = *static_cast<BufferSource*>(source)->sound;
    *format = ma_format_f32;
    *channels = data.channels;
    *sampleRate = data.sampleRate;
    if (channelMap != nullptr) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCapacity, data.channels);
    }

    return MA_SUCCESS;
}

ma_result BufferSource::getCursor(ma_data_source* source, ma_uint64* cursor) {
    *cursor = static_cast<BufferSource*>(source)->cursor.load(std::memory_order_relaxed);
    return MA_SUCCESS;
}

ma_result BufferSource::getLength(ma_data_source* source, ma_uint64* length) {
    *length = static_cast<BufferSource*>(source)->sound->frames;
    return MA_SUCCESS;
}

} // namespace haylen::audio
