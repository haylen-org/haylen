#include "audio/StreamSource.hpp"

#include <span>
#include <stdexcept>

#include "haylen/platform/AudioStream.hpp"

namespace haylen::audio {

const ma_data_source_vtable StreamSource::kVtable{&StreamSource::read, &StreamSource::seek, &StreamSource::getFormat, &StreamSource::getCursor, &StreamSource::getLength, nullptr, 0};

StreamSource::StreamSource(platform::AudioStream& value) : stream(&value), token(value.claim()) {
    ma_data_source_config config = ma_data_source_config_init();
    config.vtable = &kVtable;
    if (ma_data_source_init(&config, get()) != MA_SUCCESS) {
        throw std::runtime_error("An audio stream could not be prepared for playback.");
    }
}

StreamSource::~StreamSource() {
    ma_data_source_uninit(get());
}

// Every read fills the whole block, so the voice plays until it is stopped.
ma_result StreamSource::read(ma_data_source* source, void* frames, ma_uint64 frameCount, ma_uint64* framesRead) {
    auto& self = *static_cast<StreamSource*>(source);
    self.stream->read(self.token, std::span(static_cast<float*>(frames), static_cast<std::size_t>(frameCount) * self.stream->getChannels()));
    self.framesRead.fetch_add(frameCount, std::memory_order_relaxed);
    if (framesRead != nullptr) {
        *framesRead = frameCount;
    }
    return MA_SUCCESS;
}

ma_result StreamSource::seek(ma_data_source*, ma_uint64) {
    return MA_NOT_IMPLEMENTED;
}

ma_result StreamSource::getFormat(ma_data_source* source, ma_format* format, ma_uint32* channels, ma_uint32* sampleRate, ma_channel* channelMap, std::size_t channelMapCapacity) {
    const platform::AudioStream& stream = *static_cast<StreamSource*>(source)->stream;
    *format = ma_format_f32;
    *channels = stream.getChannels();
    *sampleRate = stream.getSampleRate();
    if (channelMap != nullptr) {
        ma_channel_map_init_standard(ma_standard_channel_map_default, channelMap, channelMapCapacity, stream.getChannels());
    }
    return MA_SUCCESS;
}

ma_result StreamSource::getCursor(ma_data_source* source, ma_uint64* cursor) {
    *cursor = static_cast<StreamSource*>(source)->framesRead.load(std::memory_order_relaxed);
    return MA_SUCCESS;
}

// A stream has no end, so it has no length either.
ma_result StreamSource::getLength(ma_data_source*, ma_uint64* length) {
    *length = 0;
    return MA_NOT_IMPLEMENTED;
}

} // namespace haylen::audio
