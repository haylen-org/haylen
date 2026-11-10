#include "support/FontCollection.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string>

namespace haylen::test {

std::vector<std::uint8_t> FontCollection::read(std::string_view name) {
    std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + std::string(name), std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::vector<std::uint8_t> FontCollection::build(std::span<const std::vector<std::uint8_t>> fonts) {
    const std::size_t header = 12 + 4 * fonts.size();
    std::vector<std::uint8_t> collection(header, 0);
    writeBigEndian(collection, 0, 0x74746366U, 4);
    writeBigEndian(collection, 4, 0x00010000U, 4);
    writeBigEndian(collection, 8, static_cast<std::uint32_t>(fonts.size()), 4);

    for (std::size_t index = 0; index < fonts.size(); ++index) {
        const std::size_t start = (collection.size() + 3) & ~std::size_t{3};
        collection.resize(start, 0);
        writeBigEndian(collection, 12 + 4 * index, static_cast<std::uint32_t>(start), 4);
        collection.insert(collection.end(), fonts[index].begin(), fonts[index].end());

        const std::uint32_t tables = readBigEndian(fonts[index], 4, 2);
        for (std::size_t record = 12; record < 12 + tables * 16; record += 16) {
            const std::uint32_t moved = readBigEndian(fonts[index], record + 8, 4) + static_cast<std::uint32_t>(start);
            writeBigEndian(collection, start + record + 8, moved, 4);
        }
    }
    return collection;
}

// The records stay sorted by tag, as readers search them, and every table moves by the size of the record added before it.
std::vector<std::uint8_t> FontCollection::addTable(std::span<const std::uint8_t> font, std::uint32_t tag) {
    const std::uint32_t tables = readBigEndian(font, 4, 2);
    std::vector<std::array<std::uint32_t, 4>> records;
    for (std::size_t record = 12; record < 12 + tables * 16; record += 16) {
        records.push_back({readBigEndian(font, record, 4), readBigEndian(font, record + 4, 4), readBigEndian(font, record + 8, 4) + 16, readBigEndian(font, record + 12, 4)});
    }
    const std::size_t end = (font.size() + 16 + 3) & ~std::size_t{3};
    records.push_back({tag, 0, static_cast<std::uint32_t>(end), 4});
    std::ranges::sort(records);

    std::vector<std::uint8_t> added(end + 4, 0);
    std::copy_n(font.begin(), 12, added.begin());
    std::copy(font.begin() + 12 + tables * 16, font.end(), added.begin() + 12 + (tables + 1) * 16);
    writeBigEndian(added, 4, tables + 1, 2);
    for (std::size_t index = 0; index < records.size(); ++index) {
        for (std::size_t field = 0; field < 4; ++field) {
            writeBigEndian(added, 12 + index * 16 + field * 4, records[index][field], 4);
        }
    }
    return added;
}

std::uint32_t FontCollection::readBigEndian(std::span<const std::uint8_t> bytes, std::size_t offset, std::size_t width) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < width; ++index) {
        value = (value << 8U) | bytes[offset + index];
    }
    return value;
}

void FontCollection::writeBigEndian(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value, std::size_t width) {
    for (std::size_t index = 0; index < width; ++index) {
        bytes[offset + index] = static_cast<std::uint8_t>(value >> (8U * (width - 1 - index)));
    }
}

} // namespace haylen::test
