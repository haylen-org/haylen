#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace haylen::content {

// Plans the byte ranges that download a set of records of one shard: records close to each other share one request, as long as the bytes between them stay small and the request stays bounded, so many small records cost few requests and a large shard is never fetched for a few of its records.
class RangePlanner final {
  public:
    // A request downloads at most this much unwanted data between two records to save the cost of another request.
    static constexpr std::uint64_t kMaximumGap = 256 * 1024;

    // No request asks for more than this, so a failed request repeats little.
    static constexpr std::uint64_t kMaximumRange = std::uint64_t{16} << 20;

    struct Span {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    // One request: its bytes and the records it covers, the run from `first` of `count` records of the input.
    struct Range {
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
        std::size_t first = 0;
        std::size_t count = 0;
    };

    // Takes the records sorted by offset, without overlaps, and returns the requests in order.
    [[nodiscard]] static std::vector<Range> plan(std::span<const Span> records);
};

} // namespace haylen::content
