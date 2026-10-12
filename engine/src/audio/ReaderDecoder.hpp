#pragma once

#include <miniaudio.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <vector>

namespace haylen::io {
class PackageReader;
}

namespace haylen::audio {

// Owns a decoder that reads an encoded file through a package reader, one window of bytes at a time, and outputs 32-bit floats in the file's own channel count and sample rate. A read of the package that fails ends the decoding, and the call that needed the bytes throws its failure. Only one thread at a time uses a decoder.
class ReaderDecoder final {
  public:
    static constexpr std::size_t kWindowSize = std::size_t{64} * 1024;

    // Reads the header of the file, and throws `std::runtime_error` when it is not a supported WAV, FLAC, MP3 or Ogg Vorbis file.
    explicit ReaderDecoder(std::shared_ptr<io::PackageReader> value);
    ~ReaderDecoder();

    ReaderDecoder(const ReaderDecoder&) = delete;
    ReaderDecoder& operator=(const ReaderDecoder&) = delete;

    [[nodiscard]] std::uint32_t getChannels() const noexcept {
        return decoder.outputChannels;
    }
    [[nodiscard]] std::uint32_t getSampleRate() const noexcept {
        return decoder.outputSampleRate;
    }

    // Returns the frames of the file, or 0 when the file does not tell them.
    [[nodiscard]] std::uint64_t getLength();

    // Decodes frames into the target, interleaved by channel, and returns how many, which is fewer than the target holds only where the file ends or its data stops decoding.
    [[nodiscard]] std::uint64_t read(std::span<float> target);

    // Moves to a frame and tells whether the file has it.
    bool seek(std::uint64_t frame);

  private:
    static constexpr std::array<std::uint8_t, 4> kOggCapture{'O', 'g', 'g', 'S'};
    static constexpr std::size_t kOggHeaderSize = 27;
    static constexpr std::size_t kOggGranuleOffset = 6;
    static constexpr std::size_t kOggLargestPage = kOggHeaderSize + 255 + std::size_t{255} * 255;

    static ma_result onRead(ma_decoder* source, void* buffer, std::size_t bytesToRead, std::size_t* bytesRead);
    static ma_result onSeek(ma_decoder* source, ma_int64 offset, ma_seek_origin origin);

    // Copies the bytes at the position through the window, and reads a range at least as large as the window straight into the target.
    [[nodiscard]] std::size_t fetch(std::span<std::uint8_t> target);
    [[nodiscard]] std::uint64_t readOggLength();
    void rethrowFailure();

    std::shared_ptr<io::PackageReader> reader;
    std::vector<std::uint8_t> window;
    std::uint64_t windowOffset = 0;
    std::size_t windowSize = 0;
    std::uint64_t position = 0;
    std::exception_ptr failure;
    ma_decoder decoder{};
};

} // namespace haylen::audio
