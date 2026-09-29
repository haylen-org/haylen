#include "text/Segmenter.hpp"

#include <algorithm>
#include <memory>
#include <string>

#include <SheenBidi/SheenBidi.h>
#include <graphemebreak.h>
#include <linebreak.h>

#include "text/PhraseBreaker.hpp"

namespace haylen::text {

std::vector<std::uint32_t> Segmenter::getScripts(std::u32string_view text) {
    std::vector<std::uint32_t> scripts(text.size(), SBScriptGetUnicodeTag(SBScriptZYYY));
    if (text.empty()) {
        return scripts;
    }

    const SBCodepointSequence sequence{SBStringEncodingUTF32, text.data(), text.size()};
    const std::unique_ptr<_SBScriptLocator, decltype(&SBScriptLocatorRelease)> locator(SBScriptLocatorCreate(), &SBScriptLocatorRelease);
    SBScriptLocatorLoadCodepoints(locator.get(), &sequence);
    while (SBScriptLocatorMoveNext(locator.get()) != 0) {
        const SBScriptAgent& agent = *SBScriptLocatorGetAgent(locator.get());
        std::fill_n(scripts.begin() + static_cast<std::ptrdiff_t>(agent.offset), agent.length, SBScriptGetUnicodeTag(agent.script));
    }
    return scripts;
}

// The libunibreak segmenter writes a break after empty text, so empty text returns before it runs.
std::vector<bool> Segmenter::getGraphemeStarts(std::u32string_view text) {
    if (text.empty()) {
        return {};
    }
    std::vector<char> breaks(text.size());
    set_graphemebreaks_utf32(reinterpret_cast<const utf32_t*>(text.data()), text.size(), nullptr, breaks.data());
    std::vector<bool> starts(text.size(), false);
    for (std::size_t index = 0; index < text.size(); ++index) {
        starts[index] = index == 0 || breaks[index - 1] == GRAPHEMEBREAK_BREAK;
    }
    return starts;
}

std::vector<Segmenter::Break> Segmenter::getLineBreaks(std::u32string_view text, std::string_view language) {
    if (text.empty()) {
        return {};
    }
    std::vector<char> breaks(text.size());
    const std::string tag(language);
    set_linebreaks_utf32(reinterpret_cast<const utf32_t*>(text.data()), text.size(), tag.empty() ? nullptr : tag.c_str(), breaks.data());
    std::vector<Break> found(text.size(), Break::Never);
    for (std::size_t index = 0; index < text.size(); ++index) {
        if (breaks[index] == LINEBREAK_MUSTBREAK) {
            found[index] = Break::Mandatory;
        } else if (breaks[index] == LINEBREAK_ALLOWBREAK) {
            found[index] = Break::Allowed;
        }
    }
    addThaiPhrases(text, found);
    return found;
}

// Thai writes no spaces between words, which the Unicode rules leave unbroken, so its runs may also break between the phrases the Thai model finds, where a grapheme cluster starts.
void Segmenter::addThaiPhrases(std::u32string_view text, std::vector<Break>& found) {
    std::vector<bool> graphemes;
    std::size_t begin = 0;
    while (begin < text.size()) {
        if (!PhraseBreaker::isThai(text[begin])) {
            ++begin;
            continue;
        }
        std::size_t end = begin + 1;
        while (end < text.size() && PhraseBreaker::isThai(text[end])) {
            ++end;
        }
        if (graphemes.empty()) {
            graphemes = getGraphemeStarts(text);
        }
        const std::vector<bool> phrases = PhraseBreaker::findThaiPhrases(text.substr(begin, end - begin));
        for (std::size_t offset = 1; offset < phrases.size(); ++offset) {
            const std::size_t index = begin + offset;
            if (phrases[offset] && graphemes[index] && found[index - 1] == Break::Never) {
                found[index - 1] = Break::Allowed;
            }
        }
        begin = end;
    }
}

bool Segmenter::isSpace(char32_t codePoint) noexcept {
    return codePoint == U'\t' || SBCodepointGetGeneralCategory(codePoint) == SBGeneralCategoryZS;
}

bool Segmenter::isParagraphSeparator(char32_t codePoint) noexcept {
    return SBCodepointGetBidiType(codePoint) == SBBidiTypeB;
}

bool Segmenter::isInvisible(char32_t codePoint) noexcept {
    const bool selector = (codePoint >= U'\U0000FE00' && codePoint <= U'\U0000FE0F') || (codePoint >= U'\U000E0100' && codePoint <= U'\U000E01EF') || (codePoint >= U'\U0000180B' && codePoint <= U'\U0000180F');
    const SBGeneralCategory category = SBCodepointGetGeneralCategory(codePoint);
    return selector || codePoint == U'\U0000034F' || category == SBGeneralCategoryCF || category == SBGeneralCategoryCC;
}

bool Segmenter::isCommon(char32_t codePoint) noexcept {
    const SBScript script = SBCodepointGetScript(codePoint);
    return script == SBScriptZYYY || script == SBScriptZINH;
}

char32_t Segmenter::getMirror(char32_t codePoint) noexcept {
    const SBCodepoint mirror = SBCodepointGetMirror(codePoint);
    return mirror != 0 ? mirror : codePoint;
}

} // namespace haylen::text
