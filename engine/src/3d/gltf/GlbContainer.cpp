#include "3d/gltf/GlbContainer.hpp"

#include <array>
#include <string>

namespace haylen::gltf {

bool GlbContainer::isGlb(std::span<const std::uint8_t> head) noexcept {
    return head.size() >= 4 && readWord(head, 0) == kMagic;
}

std::uint32_t GlbContainer::readWord(std::span<const std::uint8_t> bytes, std::size_t offset) noexcept {
    return static_cast<std::uint32_t>(bytes[offset]) | static_cast<std::uint32_t>(bytes[offset + 1]) << 8U | static_cast<std::uint32_t>(bytes[offset + 2]) << 16U | static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

GlbContainer::Layout GlbContainer::parse(std::uint64_t fileSize, const ReadAt& read, std::uint64_t maxJsonBytes, const Diagnostics& diagnostics) {
    using Problem = ReadError::Problem;

    if (fileSize < kHeaderSize) {
        diagnostics.invalid(Problem::GlbHeader, "it ends inside the header of its binary container");
    }

    std::array<std::uint8_t, kHeaderSize> header{};
    read(0, header);
    const std::uint32_t version = readWord(header, 4);
    if (version != kVersion) {
        diagnostics.refuse(Problem::GlbHeader, "is a binary container of version " + std::to_string(version) + ", while Haylen reads version 2 of glTF", Diagnostics::kExportAgain);
    }

    // A file may hold bytes after the length its header declares, which belong to no chunk.
    const std::uint64_t length = readWord(header, 8);
    if (length < kHeaderSize + kChunkHeaderSize) {
        diagnostics.invalid(Problem::GlbLength, "its header declares a length of " + std::to_string(length) + " bytes, too short for the JSON chunk");
    }

    if (length > fileSize) {
        diagnostics.invalid(Problem::GlbLength, "its header declares " + std::to_string(length) + " bytes, while the file holds " + std::to_string(fileSize));
    }

    Layout layout;
    std::uint64_t offset = kHeaderSize;
    std::size_t chunk = 0;
    while (offset < length) {
        const std::string where = Diagnostics::item("chunk", chunk);
        if (length - offset < kChunkHeaderSize) {
            diagnostics.invalid(Problem::GlbChunk, "the header of " + where + " ends at the end of the file");
        }

        std::array<std::uint8_t, kChunkHeaderSize> chunkHeader{};
        read(offset, chunkHeader);
        const std::uint64_t chunkLength = readWord(chunkHeader, 0);
        const std::uint32_t chunkType = readWord(chunkHeader, 4);
        const std::uint64_t start = offset + kChunkHeaderSize;
        if (chunkLength % 4 != 0) {
            diagnostics.invalid(Problem::GlbChunk, "the length of " + where + " is not a multiple of four bytes");
        }

        if (chunkLength > length - start) {
            diagnostics.invalid(Problem::GlbChunk, where + " runs past the end of the file");
        }

        const bool first = chunk == 0;
        if (first != (chunkType == kJsonChunk)) {
            diagnostics.invalid(Problem::GlbChunk, first ? "its first chunk is not the JSON chunk" : where + " is a second JSON chunk");
        }

        if (chunkType == kJsonChunk && chunkLength == 0) {
            diagnostics.invalid(Problem::GlbChunk, "its JSON chunk is empty");
        }

        if (chunkType == kJsonChunk) {
            layout.json = {start, chunkLength};
        }

        if (chunkType == kBinaryChunk) {
            if (chunk != 1) {
                diagnostics.invalid(Problem::GlbChunk, where + " is a binary chunk, which only the second chunk may be");
            }

            layout.binary = Range{start, chunkLength};
        }

        offset = start + chunkLength;
        ++chunk;
    }

    if (layout.json.length > maxJsonBytes) {
        diagnostics.refuse(Problem::LimitExceeded, "holds " + std::to_string(layout.json.length) + " bytes of JSON, more than the " + std::to_string(maxJsonBytes) + " bytes the reader takes", "Keep the geometry and the images of the model in its binary chunk.");
    }

    return layout;
}

} // namespace haylen::gltf
