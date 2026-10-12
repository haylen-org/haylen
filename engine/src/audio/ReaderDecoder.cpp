#include "audio/ReaderDecoder.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "haylen/io/PackageReader.hpp"

namespace haylen::audio {

ReaderDecoder::ReaderDecoder(std::shared_ptr<io::PackageReader> value) : reader(std::move(value)), window(kWindowSize) {
    const ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init(&ReaderDecoder::onRead, &ReaderDecoder::onSeek, this, &config, &decoder) != MA_SUCCESS) {
        rethrowFailure();
        throw std::runtime_error("Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.");
    }
}

ReaderDecoder::~ReaderDecoder() {
    ma_decoder_uninit(&decoder);
}

// The callbacks never let an exception into the decoder, which is C code, and a reader that failed once is not read again.
ma_result ReaderDecoder::onRead(ma_decoder* source, void* buffer, std::size_t bytesToRead, std::size_t* bytesRead) {
    auto& self = *static_cast<ReaderDecoder*>(source->pUserData);
    *bytesRead = 0;
    if (self.failure) {
        return MA_IO_ERROR;
    }
    try {
        *bytesRead = self.fetch({static_cast<std::uint8_t*>(buffer), bytesToRead});
    } catch (...) {
        self.failure = std::current_exception();
        return MA_IO_ERROR;
    }
    return *bytesRead == 0 && bytesToRead > 0 ? MA_AT_END : MA_SUCCESS;
}

// Positions outside the file move to its nearest end, as they do for a file in memory.
ma_result ReaderDecoder::onSeek(ma_decoder* source, ma_int64 offset, ma_seek_origin origin) {
    auto& self = *static_cast<ReaderDecoder*>(source->pUserData);
    const auto size = static_cast<ma_int64>(std::min<std::uint64_t>(self.reader->getSize(), std::numeric_limits<ma_int64>::max()));
    ma_int64 base = 0;
    if (origin == ma_seek_origin_current) {
        base = static_cast<ma_int64>(self.position);
    } else if (origin == ma_seek_origin_end) {
        base = size;
    }
    self.position = static_cast<std::uint64_t>(std::clamp(base + offset, ma_int64{0}, size));
    return MA_SUCCESS;
}

std::size_t ReaderDecoder::fetch(std::span<std::uint8_t> target) {
    std::size_t filled = 0;
    while (filled < target.size()) {
        const std::span<std::uint8_t> rest = target.subspan(filled);
        const bool inWindow = position >= windowOffset && position < windowOffset + windowSize;
        if (!inWindow && rest.size() >= window.size()) {
            const std::size_t count = reader->read(position, rest);
            if (count == 0) {
                break;
            }
            position += count;
            filled += count;
            continue;
        }

        if (!inWindow) {
            windowOffset = position;
            windowSize = reader->read(position, window);
            if (windowSize == 0) {
                break;
            }
        }
        const auto start = static_cast<std::size_t>(position - windowOffset);
        const std::size_t count = std::min(rest.size(), windowSize - start);
        std::copy_n(window.begin() + static_cast<std::ptrdiff_t>(start), count, rest.begin());
        position += count;
        filled += count;
    }
    return filled;
}

std::uint64_t ReaderDecoder::getLength() {
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    rethrowFailure();
    return frames > 0 ? frames : readOggLength();
}

// The Vorbis decoder reads through callbacks only by having data pushed into it, which leaves it without the length of the file, so the length comes from the granule position of the last Ogg page, which counts the frames up to the end of that page.
std::uint64_t ReaderDecoder::readOggLength() {
    const std::uint64_t size = reader->getSize();
    std::array<std::uint8_t, kOggCapture.size()> start{};
    if (size < kOggHeaderSize || reader->read(0, start) != start.size() || start != kOggCapture) {
        return 0;
    }

    std::vector<std::uint8_t> tail(static_cast<std::size_t>(std::min<std::uint64_t>(size, kOggLargestPage)));
    reader->readExactly(size - tail.size(), tail);
    for (std::size_t page = tail.size() - kOggHeaderSize + 1; page-- > 0;) {
        if (!std::equal(kOggCapture.begin(), kOggCapture.end(), tail.begin() + static_cast<std::ptrdiff_t>(page)) || tail[page + kOggCapture.size()] != 0) {
            continue;
        }
        std::uint64_t granule = 0;
        for (std::size_t index = 8; index-- > 0;) {
            granule = (granule << 8U) | tail[page + kOggGranuleOffset + index];
        }

        // A page on which no packet ends has no granule position.
        if (granule != std::numeric_limits<std::uint64_t>::max()) {
            return granule;
        }
    }
    return 0;
}

std::uint64_t ReaderDecoder::read(std::span<float> target) {
    ma_uint64 decoded = 0;
    ma_decoder_read_pcm_frames(&decoder, target.data(), target.size() / decoder.outputChannels, &decoded);
    rethrowFailure();
    return decoded;
}

bool ReaderDecoder::seek(std::uint64_t frame) {
    const ma_result result = ma_decoder_seek_to_pcm_frame(&decoder, frame);
    rethrowFailure();
    return result == MA_SUCCESS;
}

void ReaderDecoder::rethrowFailure() {
    if (failure) {
        std::rethrow_exception(failure);
    }
}

} // namespace haylen::audio
