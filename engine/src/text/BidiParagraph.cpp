#include "text/BidiParagraph.hpp"

#include <SheenBidi/SheenBidi.h>

namespace haylen::text {

// SheenBidi reads the text in place, so the paragraph keeps its analysis objects alive together.
struct BidiParagraph::Analysis {
    std::unique_ptr<const _SBAlgorithm, decltype(&SBAlgorithmRelease)> algorithm{nullptr, &SBAlgorithmRelease};
    std::unique_ptr<const _SBParagraph, decltype(&SBParagraphRelease)> paragraph{nullptr, &SBParagraphRelease};
    bool rightToLeft = false;
};

// An empty paragraph reads in the direction it was given, and left to right when that is automatic.
BidiParagraph::BidiParagraph(std::u32string_view text, Direction direction) : analysis(std::make_unique<Analysis>()) {
    analysis->rightToLeft = direction == Direction::RightToLeft;
    if (text.empty()) {
        return;
    }
    const SBCodepointSequence sequence{SBStringEncodingUTF32, text.data(), text.size()};
    analysis->algorithm.reset(SBAlgorithmCreate(&sequence));
    const SBLevel base = direction == Direction::RightToLeft ? 1 : (direction == Direction::LeftToRight ? 0 : SBLevelDefaultLTR);
    analysis->paragraph.reset(SBAlgorithmCreateParagraph(analysis->algorithm.get(), 0, text.size(), base));
    analysis->rightToLeft = SBParagraphGetBaseLevel(analysis->paragraph.get()) % 2 != 0;
}

BidiParagraph::~BidiParagraph() = default;
BidiParagraph::BidiParagraph(BidiParagraph&& other) noexcept = default;
BidiParagraph& BidiParagraph::operator=(BidiParagraph&& other) noexcept = default;

bool BidiParagraph::isRightToLeft() const noexcept {
    return analysis->rightToLeft;
}

std::uint8_t BidiParagraph::getLevel(std::size_t index) const noexcept {
    return analysis->paragraph ? SBParagraphGetLevelsPtr(analysis->paragraph.get())[index] : 0;
}

std::vector<BidiParagraph::Run> BidiParagraph::getVisualRuns(std::size_t begin, std::size_t end) const {
    std::vector<Run> runs;
    if (!analysis->paragraph || begin >= end) {
        return runs;
    }
    const std::unique_ptr<const _SBLine, decltype(&SBLineRelease)> line(SBParagraphCreateLine(analysis->paragraph.get(), begin, end - begin), &SBLineRelease);
    const SBRun* found = SBLineGetRunsPtr(line.get());
    const SBUInteger count = SBLineGetRunCount(line.get());
    runs.reserve(count);
    for (SBUInteger index = 0; index < count; ++index) {
        runs.push_back({.begin = found[index].offset, .end = found[index].offset + found[index].length, .level = found[index].level});
    }
    return runs;
}

} // namespace haylen::text
