#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "audio/OggPageIndex.hpp"

struct stb_vorbis;

namespace haylen::io {
class PackageReader;
}

namespace haylen::audio {

// Decodes an Ogg Vorbis file that it reads through a package reader into 32-bit floats in the file's own channel count and sample rate. It reads the file in windows and records its pages in an index, so a seek starts decoding at a page just before the frame and decodes forward only from there, exact to the frame. Only one thread at a time uses a decoder.
class VorbisDecoder final {
  public:
    // Tells whether the first page of a file holds the identification header of Vorbis.
    [[nodiscard]] static bool isVorbis(io::PackageReader& reader);

    // Reads the headers of the file, and throws `std::runtime_error` when they are not valid Vorbis headers.
    explicit VorbisDecoder(io::PackageReader& value);
    ~VorbisDecoder();

    VorbisDecoder(const VorbisDecoder&) = delete;
    VorbisDecoder& operator=(const VorbisDecoder&) = delete;

    [[nodiscard]] std::uint32_t getChannels() const noexcept {
        return channels;
    }
    [[nodiscard]] std::uint32_t getSampleRate() const noexcept {
        return sampleRate;
    }

    // Returns the frames of the file, or 0 when the end of the file holds no page that tells them.
    [[nodiscard]] std::uint64_t getLength();

    // Decodes frames into the target, interleaved by channel, and returns how many, which is fewer than the target holds only where the file ends or its data stops decoding.
    [[nodiscard]] std::uint64_t read(std::span<float> target);

    // Moves to a frame and tells whether the file has it.
    bool seek(std::uint64_t frame);

  private:
    struct Closer {
        void operator()(stb_vorbis* value) const noexcept;
    };

    // The first packet of an Ogg Vorbis file, its identification header, starts with these bytes.
    static constexpr std::array<std::uint8_t, 7> kIdentification{1, 'v', 'o', 'r', 'b', 'i', 's'};

    // The largest run of bytes the decoder needs at once, such as the header packets or one audio packet with the pages it spans.
    static constexpr std::size_t kLargestBuffer = std::size_t{1024} * 1024;

    void open();
    [[nodiscard]] bool resume(const OggPageIndex::Page& start);
    [[nodiscard]] bool decode();
    [[nodiscard]] bool skip(std::uint64_t frames);
    [[nodiscard]] bool fill();

    io::PackageReader& reader;
    OggPageIndex index;
    std::unique_ptr<stb_vorbis, Closer> vorbis;
    std::vector<std::uint8_t> buffer;
    std::uint64_t bufferOffset = 0;
    std::size_t bufferStart = 0;
    std::size_t bufferEnd = 0;
    std::uint32_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::uint64_t preroll = 0;

    // The frames of the packet decoded last, which the decoder owns until it decodes the next one, and the frame of the file that the next frame read comes from.
    float** packet = nullptr;
    std::size_t packetFrames = 0;
    std::size_t packetTaken = 0;
    std::int64_t packetPosition = -1;
    std::uint64_t cursor = 0;
};

} // namespace haylen::audio
