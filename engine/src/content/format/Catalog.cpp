#include "content/format/Catalog.hpp"

#include <monocypher.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include "content/Error.hpp"
#include "content/format/BinaryReader.hpp"
#include "content/format/ChunkRecord.hpp"
#include "haylen/core/Utf8.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::content {

Catalog::Catalog(std::vector<std::uint8_t> plain) noexcept : bytes(std::move(plain)) {}

Catalog::~Catalog() {
    crypto_wipe(bytes.data(), bytes.size());
}

template <std::unsigned_integral T> T Catalog::load(std::size_t offset) const noexcept {
    T value = 0;
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<T>(static_cast<T>(bytes[offset + index]) << (8 * index));
    }
    return value;
}

Catalog Catalog::parse(std::vector<std::uint8_t> plain, std::uint64_t shardCount) {
    BinaryReader reader(plain, Error::Code::CorruptCatalog, "catalog");
    if (plain.size() < kHeaderSize) {
        reader.fail("ends before its header does");
    }
    if (!std::ranges::equal(reader.readBytes(kMagic.size()), kMagic)) {
        throw Error(Error::Code::UnsupportedFormat, "The catalog does not start with the catalog magic.");
    }
    if (reader.read<std::uint16_t>() != kVersion) {
        throw Error(Error::Code::UnsupportedVersion, "The catalog has a version this app does not read.");
    }
    reader.skipZeros(sizeof(std::uint16_t));
    const auto files = reader.read<std::uint64_t>();
    const auto chunks = reader.read<std::uint64_t>();
    const auto parts = reader.read<std::uint64_t>();
    const auto strings = reader.read<std::uint64_t>();
    reader.skipZeros(kHeaderSize - reader.getPosition());

    // Each table is checked against the room left before its size is added, so hostile counts never wrap around.
    std::uint64_t total = kHeaderSize;
    for (const auto& [count, size] : {std::pair{files, kFileSize}, std::pair{chunks, kChunkSize}, std::pair{parts, kPartSize}, std::pair{strings, std::size_t{1}}}) {
        if (count > (plain.size() - total) / size) {
            reader.fail("declares tables larger than its bytes");
        }
        total += count * size;
    }
    if (total != plain.size()) {
        reader.fail("has bytes its tables do not account for");
    }

    Catalog catalog(std::move(plain));
    catalog.fileCount = files;
    catalog.chunkCount = chunks;
    catalog.partCount = parts;
    catalog.stringSize = strings;
    catalog.chunkTable = static_cast<std::size_t>(kHeaderSize + files * kFileSize);
    catalog.partTable = static_cast<std::size_t>(catalog.chunkTable + chunks * kChunkSize);
    catalog.stringTable = static_cast<std::size_t>(catalog.partTable + parts * kPartSize);
    catalog.validate(shardCount);
    return catalog;
}

bool Catalog::isValidPath(std::string_view path) {
    if (path.empty() || path.size() > kMaximumPathLength || !core::Utf8::isValid(path)) {
        return false;
    }
    if (std::ranges::any_of(path, [](char character) { return static_cast<unsigned char>(character) < 0x20 || character == 0x7F || character == '\\'; })) {
        return false;
    }
    try {
        return io::Path::normalize(path) == path;
    } catch (const std::invalid_argument&) {
        return false;
    }
}

void Catalog::validate(std::uint64_t shardCount) const {
    const auto fail = [](const std::string& problem) { throw Error(Error::Code::CorruptCatalog, "The catalog " + problem + "."); };

    for (std::uint64_t index = 0; index < chunkCount; ++index) {
        const std::size_t row = chunkTable + static_cast<std::size_t>(index) * kChunkSize;
        const auto codec = load<std::uint8_t>(row + ChunkLayout::kCodec);
        const auto profile = load<std::uint8_t>(row + ChunkLayout::kProfile);
        if (!Compression::isSupported(codec, profile) || load<std::uint16_t>(row + ChunkLayout::kReserved) != 0) {
            fail("names a codec this app does not decode");
        }
        const Chunk chunk = getChunk(index);
        if (!ChunkRecord::hasValidSizes(chunk.codec, chunk.plainSize, chunk.encodedSize)) {
            fail("declares a chunk of sizes no chunk has");
        }
        if (chunk.shard >= shardCount) {
            fail("places a chunk in a shard its manifest does not name");
        }
        if (index > 0 && !(getChunk(index - 1).storedId < chunk.storedId)) {
            fail("lists chunks out of order or twice");
        }
    }

    for (std::uint64_t index = 0; index < fileCount; ++index) {
        const std::size_t row = kHeaderSize + static_cast<std::size_t>(index) * kFileSize;
        const auto pathOffset = load<std::uint64_t>(row + FileLayout::kPathOffset);
        const auto pathLength = load<std::uint32_t>(row + FileLayout::kPathLength);
        if (pathOffset > stringSize || pathLength > stringSize - pathOffset) {
            fail("places a path outside its string table");
        }
        if (load<std::uint8_t>(row + FileLayout::kKind) != static_cast<std::uint8_t>(Kind::File) || load<std::uint8_t>(row + FileLayout::kDelivery) > static_cast<std::uint8_t>(Delivery::OnDemand) || load<std::uint16_t>(row + FileLayout::kReserved) != 0) {
            fail("declares a file of a kind or delivery this app does not know");
        }

        const File file = getFile(index);
        if (!isValidPath(file.path)) {
            fail("holds a path that is not a normalized UTF-8 package path");
        }
        if (index > 0 && !(getPath(index - 1) < file.path)) {
            fail("lists paths out of order or twice");
        }
        if (file.firstPart > partCount || file.partCount > partCount - file.firstPart) {
            fail("gives a file parts outside its part table");
        }

        // The parts of a file follow each other without gaps and end exactly at its size.
        std::uint64_t expected = 0;
        for (std::uint64_t part = file.firstPart; part < file.firstPart + file.partCount; ++part) {
            const Part piece = getPart(part);
            if (piece.chunk >= chunkCount || piece.offset != expected) {
                fail("gives a file parts that do not follow each other");
            }
            const std::uint64_t plainSize = getChunk(piece.chunk).plainSize;
            if (plainSize > std::numeric_limits<std::uint64_t>::max() - expected) {
                fail("gives a file more bytes than any file holds");
            }
            expected += plainSize;
        }
        if (expected != file.size) {
            fail("gives a file parts that do not add up to its size");
        }
    }
}

std::string_view Catalog::getPath(std::uint64_t index) const noexcept {
    const std::size_t row = kHeaderSize + static_cast<std::size_t>(index) * kFileSize;
    const auto offset = static_cast<std::size_t>(load<std::uint64_t>(row + FileLayout::kPathOffset));
    return {reinterpret_cast<const char*>(bytes.data()) + stringTable + offset, load<std::uint32_t>(row + FileLayout::kPathLength)};
}

Catalog::File Catalog::getFile(std::uint64_t index) const noexcept {
    const std::size_t row = kHeaderSize + static_cast<std::size_t>(index) * kFileSize;
    return {
        .path = getPath(index),
        .kind = static_cast<Kind>(load<std::uint8_t>(row + FileLayout::kKind)),
        .delivery = static_cast<Delivery>(load<std::uint8_t>(row + FileLayout::kDelivery)),
        .size = load<std::uint64_t>(row + FileLayout::kSize),
        .firstPart = load<std::uint64_t>(row + FileLayout::kFirstPart),
        .partCount = load<std::uint64_t>(row + FileLayout::kPartCount),
    };
}

Catalog::Chunk Catalog::getChunk(std::uint64_t index) const noexcept {
    const std::size_t row = chunkTable + static_cast<std::size_t>(index) * kChunkSize;
    const std::span<const std::uint8_t> view(bytes);
    return {
        .storedId = Digest(view.subspan(row + ChunkLayout::kStoredId).first<Digest::kSize>()),
        .contentId = Digest(view.subspan(row + ChunkLayout::kContentId).first<Digest::kSize>()),
        .plainSize = load<std::uint64_t>(row + ChunkLayout::kPlainSize),
        .encodedSize = load<std::uint64_t>(row + ChunkLayout::kEncodedSize),
        .shard = load<std::uint32_t>(row + ChunkLayout::kShard),
        .codec = static_cast<Compression::Codec>(load<std::uint8_t>(row + ChunkLayout::kCodec)),
        .profile = load<std::uint8_t>(row + ChunkLayout::kProfile),
    };
}

Catalog::Part Catalog::getPart(std::uint64_t index) const noexcept {
    const std::size_t row = partTable + static_cast<std::size_t>(index) * kPartSize;
    return {.chunk = load<std::uint64_t>(row + PartLayout::kChunk), .offset = load<std::uint64_t>(row + PartLayout::kOffset)};
}

std::uint64_t Catalog::lowerBound(std::string_view path) const noexcept {
    std::uint64_t low = 0;
    std::uint64_t high = fileCount;
    while (low < high) {
        const std::uint64_t middle = low + (high - low) / 2;
        if (getPath(middle) < path) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return low;
}

std::optional<std::uint64_t> Catalog::findFile(std::string_view path) const noexcept {
    const std::uint64_t index = lowerBound(path);
    if (index < fileCount && getPath(index) == path) {
        return index;
    }
    return std::nullopt;
}

std::pair<std::uint64_t, std::uint64_t> Catalog::findFolder(std::string_view folder) const {
    if (folder.empty()) {
        return {0, fileCount};
    }

    // Paths under a folder sort between the folder followed by a slash and the folder followed by the character after the slash.
    std::string bound(folder);
    bound.push_back('/');
    const std::uint64_t first = lowerBound(bound);
    bound.back() = '/' + 1;
    return {first, lowerBound(bound)};
}

std::uint64_t Catalog::findPart(const File& file, std::uint64_t offset) const noexcept {
    std::uint64_t low = file.firstPart;
    std::uint64_t high = file.firstPart + file.partCount;
    while (high - low > 1) {
        const std::uint64_t middle = low + (high - low) / 2;
        if (getPart(middle).offset <= offset) {
            low = middle;
        } else {
            high = middle;
        }
    }
    return low;
}

} // namespace haylen::content
