#include "NativeDemoPng.hpp"

#include <algorithm>

std::vector<std::uint8_t> NativeDemoPng::encode(std::span<const std::uint8_t> pixels, int width, int height) {
    const std::size_t row = static_cast<std::size_t>(width) * 4 + 1;
    const std::size_t raw = row * static_cast<std::size_t>(height);
    const std::size_t blocks = (raw + kBlock - 1) / kBlock;

    std::vector<std::uint8_t> file{137, 'P', 'N', 'G', '\r', '\n', 26, '\n'};
    std::vector<std::uint8_t> header;
    put32(header, static_cast<std::uint32_t>(width));
    put32(header, static_cast<std::uint32_t>(height));
    header.insert(header.end(), {8, 6, 0, 0, 0});
    appendChunk(file, "IHDR", header);

    // The zlib stream holds the rows, each after its filter byte of 0, in stored blocks, and ends with the Adler-32 of the rows.
    std::vector<std::uint8_t> stream{0x78, 0x01};
    stream.reserve(2 + raw + blocks * 5 + 4);
    std::uint32_t low = 1;
    std::uint32_t high = 0;
    std::size_t left = 0;
    for (std::size_t offset = 0; offset < raw; ++offset, --left) {
        if (left == 0) {
            left = std::min(raw - offset, kBlock);
            const auto length = static_cast<std::uint16_t>(left);
            const auto complement = static_cast<std::uint16_t>(~length);
            stream.insert(stream.end(), {static_cast<std::uint8_t>(offset + left == raw ? 1 : 0), static_cast<std::uint8_t>(length), static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(complement), static_cast<std::uint8_t>(complement >> 8)});
        }
        const std::size_t column = offset % row;
        const std::uint8_t byte = column == 0 ? 0 : pixels[(offset / row) * (row - 1) + column - 1];
        stream.push_back(byte);
        low = (low + byte) % 65521;
        high = (high + low) % 65521;
    }
    put32(stream, (high << 16) | low);
    appendChunk(file, "IDAT", stream);
    appendChunk(file, "IEND", {});
    return file;
}

std::uint32_t NativeDemoPng::crc(std::span<const std::uint8_t> bytes) {
    std::uint32_t value = ~0U;
    for (const std::uint8_t byte : bytes) {
        value ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            value = (value >> 1) ^ (0xEDB88320U & (0U - (value & 1U)));
        }
    }
    return ~value;
}

void NativeDemoPng::put32(std::vector<std::uint8_t>& file, std::uint32_t value) {
    file.insert(file.end(), {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)});
}

void NativeDemoPng::appendChunk(std::vector<std::uint8_t>& file, std::string_view type, std::span<const std::uint8_t> data) {
    put32(file, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = file.size();
    file.insert(file.end(), type.begin(), type.end());
    file.insert(file.end(), data.begin(), data.end());
    put32(file, crc(std::span(file).subspan(start)));
}
