#include "audio/OggPageIndex.hpp"

#include <algorithm>
#include <iterator>

#include "haylen/io/PackageReader.hpp"

namespace haylen::audio {

consteval std::array<std::uint32_t, 256> OggPageIndex::makeChecksumTable() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t index = 0; index < table.size(); ++index) {
        std::uint32_t value = index << 24U;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value & 0x80000000U) != 0 ? (value << 1U) ^ 0x04C11DB7U : value << 1U;
        }
        table[index] = value;
    }
    return table;
}

const std::array<std::uint32_t, 256> OggPageIndex::kChecksumTable = makeChecksumTable();

bool OggPageIndex::startsWith(io::PackageReader& reader, std::span<const std::uint8_t> bytes) {
    std::array<std::uint8_t, kLargestHeader> start{};
    const std::span<const std::uint8_t> head = std::span(start).first(reader.read(0, start));
    if (!holdsHeader(head) || !parse(head)) {
        return false;
    }
    const std::size_t packet = kHeaderSize + head[kSegmentsOffset];
    std::vector<std::uint8_t> first(bytes.size());
    return reader.read(packet, first) == first.size() && std::ranges::equal(first, bytes);
}

OggPageIndex::OggPageIndex(io::PackageReader& value) : reader(value) {}

void OggPageIndex::restart(std::uint64_t offset) noexcept {
    recording = offset;
}

void OggPageIndex::record(std::uint64_t offset, std::span<const std::uint8_t> bytes) {
    while (recording) {
        if (*recording < offset) {
            recording.reset();
            return;
        }
        if (*recording - offset >= bytes.size()) {
            return;
        }

        const std::span<const std::uint8_t> rest = bytes.subspan(static_cast<std::size_t>(*recording - offset));
        if (!holdsHeader(rest)) {
            return;
        }
        const std::optional<Header> parsed = parse(rest);
        if (!parsed) {
            recording.reset();
            return;
        }
        if (parsed->resumable) {
            add({*recording, parsed->granule});
        }
        *recording += parsed->size;
    }
}

std::optional<OggPageIndex::Page> OggPageIndex::find(std::uint64_t frame) {
    const auto after = std::ranges::upper_bound(pages, frame, std::ranges::less{}, &Page::frames);
    std::optional<Page> best;
    if (after != pages.begin()) {
        best = *std::prev(after);
    }
    std::uint64_t low = best ? best->offset : 0;
    std::uint64_t high = after != pages.end() ? after->offset : reader.getSize();

    // The best page lies between the known pages around the frame, and halving a wide span finds it in a few reads.
    while (high - low > kHopSpan) {
        const std::uint64_t middle = low + (high - low) / 2;
        const std::optional<Page> found = synchronize(middle, high);
        if (found && found->frames <= frame) {
            best = found;
            low = found->offset;
            continue;
        }
        high = middle;
    }

    // A short span is read header by header, since every page in it starts where the one before it ends.
    for (std::uint64_t offset = low; offset < high;) {
        const std::optional<Header> parsed = readHeader(offset);
        if (!parsed) {
            break;
        }
        if (parsed->resumable) {
            if (parsed->granule > frame) {
                break;
            }
            best = Page{offset, parsed->granule};
            add(*best);
        }
        offset += parsed->size;
    }
    return best;
}

std::uint64_t OggPageIndex::readLength() {
    const std::uint64_t size = reader.getSize();
    pageBytes.resize(static_cast<std::size_t>(std::min<std::uint64_t>(size, kLargestPage)));
    reader.readExactly(size - pageBytes.size(), pageBytes);

    // The granule position of the last page that a packet ends on counts the frames up to the end of the file.
    for (std::size_t start = pageBytes.size(); start-- > 0;) {
        const std::span<const std::uint8_t> rest = std::span(pageBytes).subspan(start);
        if (!holdsHeader(rest)) {
            continue;
        }
        const std::optional<Header> parsed = parse(rest);
        if (parsed && parsed->size <= rest.size() && parsed->granule != kNoGranule && checksum(rest.first(static_cast<std::size_t>(parsed->size))) == readNumber(rest.subspan(kChecksumOffset), 4)) {
            return parsed->granule;
        }
    }
    return 0;
}

std::optional<OggPageIndex::Header> OggPageIndex::parse(std::span<const std::uint8_t> bytes) {
    if (!std::ranges::equal(bytes.first(kCapture.size()), kCapture) || bytes[kCapture.size()] != 0) {
        return std::nullopt;
    }
    const std::size_t count = bytes[kSegmentsOffset];
    const std::span<const std::uint8_t> lacing = bytes.subspan(kHeaderSize, count);
    Header parsed;
    parsed.granule = readNumber(bytes.subspan(kGranuleOffset), 8);
    parsed.size = kHeaderSize + count;
    for (const std::uint8_t length : lacing) {
        parsed.size += length;
    }

    // A granule position of zero belongs to the header packets, after which decoding starts only from the start of the file.
    parsed.resumable = count > 0 && lacing.back() < 255 && parsed.granule > 0 && parsed.granule <= kLargestFrames;
    return parsed;
}

bool OggPageIndex::holdsHeader(std::span<const std::uint8_t> bytes) noexcept {
    return bytes.size() >= kHeaderSize && bytes.size() >= kHeaderSize + bytes[kSegmentsOffset];
}

std::uint64_t OggPageIndex::readNumber(std::span<const std::uint8_t> bytes, std::size_t count) noexcept {
    std::uint64_t value = 0;
    for (std::size_t index = count; index-- > 0;) {
        value = (value << 8U) | bytes[index];
    }
    return value;
}

// The checksum of a page covers the whole page with the bytes of the checksum itself as zeros.
std::uint32_t OggPageIndex::checksum(std::span<const std::uint8_t> page) noexcept {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < page.size(); ++index) {
        const std::uint8_t byte = index >= kChecksumOffset && index < kChecksumOffset + 4 ? 0 : page[index];
        value = (value << 8U) ^ kChecksumTable[((value >> 24U) ^ byte) & 0xFFU];
    }
    return value;
}

std::optional<OggPageIndex::Header> OggPageIndex::readHeader(std::uint64_t offset) {
    const std::span<std::uint8_t> bytes = std::span(headerBytes).first(reader.read(offset, headerBytes));
    return holdsHeader(bytes) ? parse(bytes) : std::nullopt;
}

// Reads a whole page and checks its checksum, which tells a page from bytes inside a packet that only look like one.
std::optional<OggPageIndex::Header> OggPageIndex::readCheckedPage(std::uint64_t offset) {
    const std::optional<Header> parsed = readHeader(offset);
    if (!parsed || offset + parsed->size > reader.getSize()) {
        return std::nullopt;
    }
    pageBytes.resize(static_cast<std::size_t>(parsed->size));
    reader.readExactly(offset, pageBytes);
    return checksum(pageBytes) == readNumber(std::span(pageBytes).subspan(kChecksumOffset), 4) ? parsed : std::nullopt;
}

// Finds the first page after which decoding can start between two offsets of the file, and records it.
std::optional<OggPageIndex::Page> OggPageIndex::synchronize(std::uint64_t from, std::uint64_t to) {
    probeBytes.resize(kProbeSize);
    for (std::uint64_t offset = from; offset < to;) {
        const std::size_t count = reader.read(offset, std::span(probeBytes).first(static_cast<std::size_t>(std::min<std::uint64_t>(kProbeSize, to - offset + kCapture.size() - 1))));
        if (count < kCapture.size()) {
            return std::nullopt;
        }

        for (std::size_t start = 0; start + kCapture.size() <= count && offset + start < to; ++start) {
            if (!std::equal(kCapture.begin(), kCapture.end(), probeBytes.begin() + static_cast<std::ptrdiff_t>(start))) {
                continue;
            }
            std::optional<Header> parsed = readCheckedPage(offset + start);
            if (!parsed) {
                continue;
            }

            // Once one page is found, the next pages start where each one ends.
            std::uint64_t at = offset + start;
            while (parsed && !parsed->resumable) {
                at += parsed->size;
                parsed = at < to ? readHeader(at) : std::nullopt;
            }
            if (!parsed) {
                return std::nullopt;
            }
            const Page found{at, parsed->granule};
            add(found);
            return found;
        }
        offset += count - (kCapture.size() - 1);
    }
    return std::nullopt;
}

void OggPageIndex::add(const Page& page) {
    const auto at = std::ranges::lower_bound(pages, page.offset, std::ranges::less{}, &Page::offset);
    if (at != pages.end() && at->offset == page.offset) {
        return;
    }
    pages.insert(at, page);
}

} // namespace haylen::audio
