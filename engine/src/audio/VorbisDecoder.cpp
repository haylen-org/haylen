#include "audio/VorbisDecoder.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include "audio/ReaderDecoder.hpp"
#include "haylen/io/PackageReader.hpp"

// The declarations of the Vorbis decoder that miniaudio compiles in `MiniaudioImpl.c`.
#define STB_VORBIS_HEADER_ONLY
#include <extras/stb_vorbis.c>

namespace haylen::audio {

bool VorbisDecoder::isVorbis(io::PackageReader& reader) {
    return OggPageIndex::startsWith(reader, kIdentification);
}

VorbisDecoder::VorbisDecoder(io::PackageReader& value) : reader(value), index(value), buffer(ReaderDecoder::kWindowSize) {
    open();
    const stb_vorbis_info info = stb_vorbis_get_info(vorbis.get());
    channels = static_cast<std::uint32_t>(info.channels);
    sampleRate = info.sample_rate;
    preroll = static_cast<std::uint64_t>(info.max_frame_size);
}

VorbisDecoder::~VorbisDecoder() = default;

void VorbisDecoder::Closer::operator()(stb_vorbis* value) const noexcept {
    stb_vorbis_close(value);
}

std::uint64_t VorbisDecoder::getLength() {
    return index.readLength();
}

std::uint64_t VorbisDecoder::read(std::span<float> target) {
    const std::size_t frames = target.size() / channels;
    std::size_t done = 0;
    while (done < frames) {
        if (packetTaken == packetFrames && !decode()) {
            break;
        }
        const std::size_t count = std::min(frames - done, packetFrames - packetTaken);
        for (std::size_t frame = 0; frame < count; ++frame) {
            for (std::size_t channel = 0; channel < channels; ++channel) {
                target[(done + frame) * channels + channel] = packet[channel][packetTaken + frame];
            }
        }
        packetTaken += count;
        done += count;
    }
    cursor += done;
    return done;
}

bool VorbisDecoder::seek(std::uint64_t frame) {
    // The first packet after a page only primes the decoder, so decoding starts after a page that ends at least that packet before the frame.
    const std::optional<OggPageIndex::Page> start = frame >= preroll ? index.find(frame - preroll) : std::nullopt;
    const bool ahead = frame >= cursor && (!start || start->frames <= cursor);

    // Decoding goes back to the start of the file for a frame before every page, and also when the decoder cannot place the frames after the page, as in a damaged file.
    if (!ahead && (!start || !resume(*start) || cursor > frame)) {
        open();
    }
    return skip(frame - cursor);
}

void VorbisDecoder::open() {
    vorbis.reset();
    index.restart(0);
    bufferOffset = 0;
    bufferStart = 0;
    bufferEnd = 0;
    packetFrames = 0;
    packetTaken = 0;
    cursor = 0;

    // The header packets must be whole before the decoder opens, and it reports how many bytes they took.
    while (fill()) {
        int used = 0;
        int error = VORBIS__no_error;
        stb_vorbis* opened = stb_vorbis_open_pushdata(buffer.data(), static_cast<int>(bufferEnd), &used, &error, nullptr);
        if (opened != nullptr) {
            vorbis.reset(opened);
            bufferStart = static_cast<std::size_t>(used);
            return;
        }
        if (error != VORBIS_need_more_data) {
            break;
        }
    }
    throw std::runtime_error("Audio data is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.");
}

// Starts decoding after a page, and tells whether the decoder placed the first frame it decoded from there.
bool VorbisDecoder::resume(const OggPageIndex::Page& start) {
    stb_vorbis_flush_pushdata(vorbis.get());
    index.restart(start.offset);
    bufferOffset = start.offset;
    bufferStart = 0;
    bufferEnd = 0;
    packetFrames = 0;
    packetTaken = 0;
    if (!decode() || packetPosition < 0) {
        return false;
    }
    cursor = static_cast<std::uint64_t>(packetPosition);
    return true;
}

// Decodes packets until one gives frames, and tells whether one did before the file ended or its data stopped decoding. After a flush the decoder finds the page it starts on, takes its granule position and decodes one packet without frames, and from then on it counts the frame where each packet starts.
bool VorbisDecoder::decode() {
    while (true) {
        const int position = stb_vorbis_get_sample_offset(vorbis.get());
        int frames = 0;
        float** output = nullptr;
        const int used = stb_vorbis_decode_frame_pushdata(vorbis.get(), buffer.data() + bufferStart, static_cast<int>(bufferEnd - bufferStart), nullptr, &output, &frames);
        if (used == 0) {
            const int error = stb_vorbis_get_error(vorbis.get());
            if ((error != VORBIS__no_error && error != VORBIS_need_more_data) || !fill()) {
                return false;
            }
            continue;
        }

        bufferStart += static_cast<std::size_t>(used);
        if (frames > 0) {
            packet = output;
            packetFrames = static_cast<std::size_t>(frames);
            packetTaken = 0;
            packetPosition = position;
            return true;
        }
    }
}

// Decodes and drops frames, and tells whether the file held them all.
bool VorbisDecoder::skip(std::uint64_t frames) {
    while (frames > 0) {
        if (packetTaken == packetFrames && !decode()) {
            return false;
        }
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(frames, packetFrames - packetTaken));
        packetTaken += count;
        cursor += count;
        frames -= count;
    }
    return true;
}

// Moves the bytes the decoder has not used to the front of the buffer and reads more of the file after them, and tells whether the file had more.
bool VorbisDecoder::fill() {
    if (bufferStart > 0) {
        std::copy(buffer.begin() + static_cast<std::ptrdiff_t>(bufferStart), buffer.begin() + static_cast<std::ptrdiff_t>(bufferEnd), buffer.begin());
        bufferOffset += bufferStart;
        bufferEnd -= bufferStart;
        bufferStart = 0;
    }
    if (bufferEnd == buffer.size()) {
        if (buffer.size() >= kLargestBuffer) {
            throw std::runtime_error("The Ogg Vorbis file holds a packet larger than 1 MiB, the most that a streamed sound reads at once.");
        }
        buffer.resize(buffer.size() * 2);
    }

    const std::size_t count = reader.read(bufferOffset + bufferEnd, std::span(buffer).subspan(bufferEnd));
    if (count == 0) {
        return false;
    }
    bufferEnd += count;
    index.record(bufferOffset, std::span(buffer).first(bufferEnd));
    return true;
}

} // namespace haylen::audio
