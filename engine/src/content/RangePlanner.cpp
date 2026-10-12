#include "content/RangePlanner.hpp"

namespace haylen::content {

std::vector<RangePlanner::Range> RangePlanner::plan(std::span<const Span> records) {
    std::vector<Range> ranges;
    for (std::size_t index = 0; index < records.size(); ++index) {
        const Span& record = records[index];
        if (!ranges.empty()) {
            Range& last = ranges.back();
            const std::uint64_t end = last.offset + last.size;
            const std::uint64_t joined = record.offset + record.size - last.offset;
            if (record.offset - end <= kMaximumGap && joined <= kMaximumRange) {
                last.size = joined;
                ++last.count;
                continue;
            }
        }
        ranges.push_back({.offset = record.offset, .size = record.size, .first = index, .count = 1});
    }
    return ranges;
}

} // namespace haylen::content
