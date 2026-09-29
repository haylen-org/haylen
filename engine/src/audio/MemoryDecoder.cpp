#include "audio/MemoryDecoder.hpp"

#include <stdexcept>

namespace haylen::audio {

MemoryDecoder::MemoryDecoder(std::span<const std::uint8_t> encoded) {
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (encoded.empty() || ma_decoder_init_memory(encoded.data(), encoded.size(), &config, &decoder) != MA_SUCCESS) {
        throw std::runtime_error("Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.");
    }
}

MemoryDecoder::~MemoryDecoder() {
    ma_decoder_uninit(&decoder);
}

} // namespace haylen::audio
