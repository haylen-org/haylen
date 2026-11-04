#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "audio/OggPageIndex.hpp"
#include "io/MemoryReader.hpp"

namespace haylen::audio {

// Finds pages of Ogg files that the index never read in order, also in files damaged on purpose, which it must read without reading outside them or failing.
class OggPageIndexTest : public ::testing::Test {
  protected:
    [[nodiscard]] static std::vector<std::uint8_t> readTestFile(const std::string& name) {
        std::ifstream stream(std::filesystem::path(HAYLEN_TEST_AUDIO) / name, std::ios::binary);
        return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
    }

    [[nodiscard]] static std::shared_ptr<io::PackageReader> makeReader(std::vector<std::uint8_t> file) {
        return std::make_shared<io::MemoryReader>(std::make_shared<const std::vector<std::uint8_t>>(std::move(file)));
    }

    // The audio pages of a whole file, found by walking its pages from the first, with the granule position of each.
    [[nodiscard]] static std::vector<OggPageIndex::Page> walk(const std::vector<std::uint8_t>& file) {
        std::vector<OggPageIndex::Page> pages;
        for (std::size_t offset = 0; offset + 27 <= file.size();) {
            const std::size_t count = file[offset + 26];
            std::size_t size = 27 + count;
            for (std::size_t segment = 0; segment < count; ++segment) {
                size += file[offset + 27 + segment];
            }
            std::uint64_t granule = 0;
            for (std::size_t index = 8; index-- > 0;) {
                granule = (granule << 8U) | file[offset + 6 + index];
            }
            if (count > 0 && file[offset + 26 + count] < 255 && granule > 0 && granule != ~std::uint64_t{0}) {
                pages.push_back({offset, granule});
            }
            offset += size;
        }
        return pages;
    }

    // Writes the checksum of the page at the offset, which covers the whole page with the bytes of the checksum as zeros.
    static void sign(std::vector<std::uint8_t>& file, std::size_t offset, std::size_t size) {
        std::fill_n(file.begin() + static_cast<std::ptrdiff_t>(offset + 22), 4, std::uint8_t{0});
        std::uint32_t value = 0;
        for (std::size_t index = offset; index < offset + size; ++index) {
            value ^= static_cast<std::uint32_t>(file[index]) << 24U;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 0x80000000U) != 0 ? (value << 1U) ^ 0x04C11DB7U : value << 1U;
            }
        }
        for (std::size_t index = 0; index < 4; ++index) {
            file[offset + 22 + index] = static_cast<std::uint8_t>(value >> (8 * index));
        }
    }

    // The page that a search for the frame should find: the last one that ends at or before it.
    [[nodiscard]] static std::optional<OggPageIndex::Page> expected(const std::vector<OggPageIndex::Page>& pages, std::uint64_t frame) {
        std::optional<OggPageIndex::Page> found;
        for (const OggPageIndex::Page& page : pages) {
            if (page.frames <= frame) {
                found = page;
            }
        }
        return found;
    }

    static void expectSame(const std::optional<OggPageIndex::Page>& found, const std::optional<OggPageIndex::Page>& expected, std::uint64_t frame) {
        ASSERT_EQ(found.has_value(), expected.has_value()) << frame;
        if (expected) {
            EXPECT_EQ(found->offset, expected->offset) << frame;
            EXPECT_EQ(found->frames, expected->frames) << frame;
        }
    }
};

TEST_F(OggPageIndexTest, FindsTheLastPageBeforeAFrameInAFileItNeverRead) {
    const std::vector<std::uint8_t> file = readTestFile("notes.ogg");
    const std::vector<OggPageIndex::Page> pages = walk(file);
    ASSERT_GT(pages.size(), 20U);

    // Every search of a new index bisects the file, and the searches of one index go back and forth through what it learned.
    const std::shared_ptr<io::PackageReader> reader = makeReader(file);
    OggPageIndex index(*reader);
    for (std::uint64_t step = 0; step <= 50; ++step) {
        const std::uint64_t frame = (step * 7919 % 51) * (pages.back().frames / 50);
        expectSame(index.find(frame), expected(pages, frame), frame);
        OggPageIndex fresh(*reader);
        expectSame(fresh.find(frame), expected(pages, frame), frame);
    }
    EXPECT_FALSE(index.find(pages.front().frames - 1));
    EXPECT_EQ(index.readLength(), pages.back().frames);
}

TEST_F(OggPageIndexTest, SkipsBytesInsidePagesThatOnlyLookLikeAPage) {
    const std::vector<std::uint8_t> file = readTestFile("notes.ogg");
    const std::vector<OggPageIndex::Page> pages = walk(file);

    // Every few hundred bytes inside every page lies a copy of its header with another granule position, which only its checksum tells from a page, while the page around them keeps a valid checksum.
    std::vector<std::uint8_t> damaged = file;
    for (std::size_t index = 0; index + 1 < pages.size(); ++index) {
        const std::size_t real = pages[index].offset;
        const std::size_t end = pages[index + 1].offset;
        const std::size_t header = 27 + file[real + 26];
        for (std::size_t fake = real + 300; fake + header < end; fake += 300) {
            std::copy_n(file.begin() + static_cast<std::ptrdiff_t>(real), header, damaged.begin() + static_cast<std::ptrdiff_t>(fake));
            damaged[fake + 6] ^= 0x40U;
        }
        sign(damaged, real, end - real);
    }

    const std::shared_ptr<io::PackageReader> reader = makeReader(damaged);
    for (std::uint64_t step = 1; step <= 40; ++step) {
        const std::uint64_t frame = step * (pages.back().frames / 40);
        OggPageIndex index(*reader);
        expectSame(index.find(frame), expected(pages, frame), frame);
    }
}

TEST_F(OggPageIndexTest, ReadsDamagedFilesWithoutFailing) {
    const std::vector<std::uint8_t> file = readTestFile("notes.ogg");
    std::vector<std::vector<std::uint8_t>> files;
    for (const std::size_t cut : {std::size_t{0}, std::size_t{3}, std::size_t{27}, std::size_t{60}, std::size_t{5000}, file.size() / 2, file.size() - 1}) {
        files.emplace_back(file.begin(), file.begin() + static_cast<std::ptrdiff_t>(cut));
    }

    // Headers that claim the largest pages, granule positions of every bit and capture patterns with nothing after them.
    std::vector<std::uint8_t> largest = file;
    for (std::size_t offset = 0; offset + 27 + 255 <= largest.size(); offset += 4096) {
        std::copy_n(file.begin(), 4, largest.begin() + static_cast<std::ptrdiff_t>(offset));
        largest[offset + 4] = 0;
        std::fill_n(largest.begin() + static_cast<std::ptrdiff_t>(offset + 6), 8, std::uint8_t{0xFF});
        largest[offset + 26] = 255;
        std::fill_n(largest.begin() + static_cast<std::ptrdiff_t>(offset + 27), 255, std::uint8_t{255});
    }
    files.push_back(std::move(largest));
    std::vector<std::uint8_t> captures;
    for (int index = 0; index < 20000; ++index) {
        captures.insert(captures.end(), {'O', 'g', 'g', 'S'});
    }
    files.push_back(std::move(captures));

    for (const std::vector<std::uint8_t>& bytes : files) {
        SCOPED_TRACE(bytes.size());
        const std::shared_ptr<io::PackageReader> reader = makeReader(bytes);
        OggPageIndex index(*reader);
        EXPECT_NO_THROW((void)OggPageIndex::startsWith(*reader, std::span(file).first(7)));
        EXPECT_NO_THROW((void)index.readLength());
        index.restart(0);
        index.record(0, bytes);
        for (const std::uint64_t frame : {std::uint64_t{0}, std::uint64_t{50000}, std::uint64_t{1} << 40U}) {
            const std::optional<OggPageIndex::Page> found = index.find(frame);
            if (found) {
                EXPECT_LT(found->offset, bytes.size());
                EXPECT_LE(found->frames, frame);
            }
        }
    }
}

} // namespace haylen::audio
