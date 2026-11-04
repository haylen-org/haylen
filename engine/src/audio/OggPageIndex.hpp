#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace haylen::io {
class PackageReader;
}

namespace haylen::audio {

// The pages of an Ogg file after which decoding can start, each with its offset and the frames that the file holds up to its end. Decoding in order records the pages it reads, and a search for a frame that the known pages do not bound closely bisects the file through range reads and then hops the page headers of a short span, so a seek reads a few pages of a file of any length. Only one thread at a time uses an index.
class OggPageIndex final {
  public:
    // A page whose last packet ends on it, with the granule position of that packet, the frames decoded up to its end, so the next page starts a packet.
    struct Page {
        std::uint64_t offset = 0;
        std::uint64_t frames = 0;
    };

    // Tells whether a file starts with an Ogg page whose first packet starts with the bytes.
    [[nodiscard]] static bool startsWith(io::PackageReader& reader, std::span<const std::uint8_t> bytes);

    explicit OggPageIndex(io::PackageReader& value);

    // Records pages again from an offset where a page starts, as decoding moves there.
    void restart(std::uint64_t offset) noexcept;

    // Records the pages whose headers the bytes of the file at the offset hold whole, on from the page where recording stands, and stops recording at bytes that are not a page.
    void record(std::uint64_t offset, std::span<const std::uint8_t> bytes);

    // Returns the last page that ends at or before the frame, or none when the frame comes before every such page.
    [[nodiscard]] std::optional<Page> find(std::uint64_t frame);

    // Returns the frames of the file from the granule position of its last whole page, or 0 when the end of the file holds none.
    [[nodiscard]] std::uint64_t readLength();

  private:
    struct Header {
        std::uint64_t granule = 0;
        std::uint64_t size = 0;
        // Tells whether decoding can start after the page: its last packet ends on it with a granule position that the decoder counts.
        bool resumable = false;
    };

    static constexpr std::array<std::uint8_t, 4> kCapture{'O', 'g', 'g', 'S'};
    static constexpr std::size_t kHeaderSize = 27;
    static constexpr std::size_t kGranuleOffset = 6;
    static constexpr std::size_t kChecksumOffset = 22;
    static constexpr std::size_t kSegmentsOffset = 26;
    static constexpr std::size_t kLargestHeader = kHeaderSize + 255;
    static constexpr std::size_t kLargestPage = kLargestHeader + std::size_t{255} * 255;
    static constexpr std::uint64_t kNoGranule = ~std::uint64_t{0};

    // The decoder counts frames in 31 bits, so pages past that count never start decoding.
    static constexpr std::uint64_t kLargestFrames = 0x7FFFFFFF;

    // A span of the file no wider than this is searched by hopping its page headers instead of bisecting it, and a bisection reads this much at a time to find a page.
    static constexpr std::uint64_t kHopSpan = std::uint64_t{16} * 1024;
    static constexpr std::size_t kProbeSize = std::size_t{8} * 1024;

    static const std::array<std::uint32_t, 256> kChecksumTable;

    static consteval std::array<std::uint32_t, 256> makeChecksumTable();

    // Parses the header at the start of the bytes, which hold it whole, or returns none when they are not a page.
    [[nodiscard]] static std::optional<Header> parse(std::span<const std::uint8_t> bytes);
    [[nodiscard]] static bool holdsHeader(std::span<const std::uint8_t> bytes) noexcept;
    [[nodiscard]] static std::uint64_t readNumber(std::span<const std::uint8_t> bytes, std::size_t count) noexcept;
    [[nodiscard]] static std::uint32_t checksum(std::span<const std::uint8_t> page) noexcept;

    [[nodiscard]] std::optional<Header> readHeader(std::uint64_t offset);
    [[nodiscard]] std::optional<Header> readCheckedPage(std::uint64_t offset);
    [[nodiscard]] std::optional<Page> synchronize(std::uint64_t from, std::uint64_t to);
    void add(const Page& page);

    io::PackageReader& reader;
    std::vector<Page> pages;
    std::optional<std::uint64_t> recording;
    std::array<std::uint8_t, kLargestHeader> headerBytes{};
    std::vector<std::uint8_t> probeBytes;
    std::vector<std::uint8_t> pageBytes;
};

} // namespace haylen::audio
