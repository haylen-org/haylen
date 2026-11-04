#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <vector>

#include "audio/OggPageIndex.hpp"
#include "io/MemoryReader.hpp"

// The entry point libFuzzer calls with each input it generates, which is an Ogg file for the index of its pages. The index tells whether its first packet starts like Vorbis, reads its length, records its pages as decoding in order does and searches it for frames up to past the largest granule position, from what it recorded and in a new index that bisects the file. Every page it finds must start inside the file and end at or before the frame.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    static constexpr std::array<std::uint8_t, 7> kVorbis{1, 'v', 'o', 'r', 'b', 'i', 's'};
    static constexpr std::array<std::uint64_t, 6> kFrames{0, 1, 4096, 1'000'000, 0x7FFFFFFF, std::uint64_t{1} << 40U};
    auto bytes = std::make_shared<const std::vector<std::uint8_t>>(data, data + size);
    haylen::io::MemoryReader reader(bytes);
    (void)haylen::audio::OggPageIndex::startsWith(reader, kVorbis);

    haylen::audio::OggPageIndex recorded(reader);
    (void)recorded.readLength();
    recorded.restart(0);
    recorded.record(0, *bytes);
    for (const std::uint64_t frame : kFrames) {
        haylen::audio::OggPageIndex fresh(reader);
        for (haylen::audio::OggPageIndex* index : {&recorded, &fresh}) {
            const std::optional<haylen::audio::OggPageIndex::Page> found = index->find(frame);
            if (found && (found->offset >= size || found->frames > frame)) {
                std::abort();
            }
        }
    }
    return 0;
}
