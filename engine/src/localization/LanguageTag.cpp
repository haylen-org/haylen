#include "localization/LanguageTag.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace haylen::localization {

bool LanguageTag::isLetters(std::string_view subtag) noexcept {
    return std::ranges::all_of(subtag, [](char character) { return std::isalpha(static_cast<unsigned char>(character)) != 0; });
}

bool LanguageTag::isDigits(std::string_view subtag) noexcept {
    return std::ranges::all_of(subtag, [](char character) { return std::isdigit(static_cast<unsigned char>(character)) != 0; });
}

bool LanguageTag::isAlphanumeric(std::string_view subtag) noexcept {
    return std::ranges::all_of(subtag, [](char character) { return std::isalnum(static_cast<unsigned char>(character)) != 0; });
}

// A region is two letters, such as `BR`, or three digits, such as the `419` of Latin America.
bool LanguageTag::isRegion(std::string_view subtag) noexcept {
    return (subtag.size() == 2 && isLetters(subtag)) || (subtag.size() == 3 && isDigits(subtag));
}

// A variant is five to eight letters or digits, such as `valencia`, or four that start with a digit, such as `1901`.
bool LanguageTag::isVariant(std::string_view subtag) noexcept {
    const bool startsWithDigit = !subtag.empty() && std::isdigit(static_cast<unsigned char>(subtag.front())) != 0;
    return (subtag.size() >= 5 && subtag.size() <= 8) || (subtag.size() == 4 && startsWithDigit);
}

void LanguageTag::toLower(std::string& text) noexcept {
    for (char& character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
}

void LanguageTag::toUpper(std::string& text) noexcept {
    for (char& character : text) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
}

std::optional<std::string> LanguageTag::parse(std::string_view tag) {
    const std::size_t languageEnd = std::min(tag.find_first_of("-_"), tag.size());
    const std::string_view language = tag.substr(0, languageEnd);
    if (language.size() < 2 || language.size() > 3 || !isLetters(language)) {
        return std::nullopt;
    }
    std::string canonical(language);
    toLower(canonical);

    // Every subtag after the language follows the parts before it: the script, then the region, then the variants.
    Part expected = Part::Script;
    for (std::size_t start = languageEnd + 1; start <= tag.size();) {
        const std::size_t end = std::min(tag.find_first_of("-_", start), tag.size());
        const std::string_view subtag = tag.substr(start, end - start);
        start = end + 1;
        if (subtag.empty() || subtag.size() > 8 || !isAlphanumeric(subtag)) {
            return std::nullopt;
        }
        if (subtag.size() == 1) {
            break;
        }

        std::string written(subtag);
        toLower(written);
        if (expected == Part::Script && subtag.size() == 4 && isLetters(subtag)) {
            written.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(written.front())));
            expected = Part::Region;
        } else if (expected != Part::Variant && isRegion(subtag)) {
            toUpper(written);
            expected = Part::Variant;
        } else if (isVariant(subtag)) {
            expected = Part::Variant;
        } else {
            return std::nullopt;
        }
        canonical += '-';
        canonical += written;
    }
    return canonical;
}

std::string LanguageTag::normalize(std::string_view tag) {
    std::optional<std::string> canonical = parse(tag);
    if (!canonical) {
        throw std::invalid_argument("The language tag \"" + std::string(tag) + "\" is not valid. Write a language such as \"en\", with a script or a region where it needs one, such as \"zh-Hans\", \"pt-BR\" or \"pt_BR\".");
    }
    return std::move(*canonical);
}

std::string_view LanguageTag::getParent(std::string_view tag) noexcept {
    const std::size_t last = tag.rfind('-');
    return last == std::string_view::npos ? std::string_view() : tag.substr(0, last);
}

std::string_view LanguageTag::getLanguage(std::string_view tag) noexcept {
    return tag.substr(0, tag.find('-'));
}

std::string_view LanguageTag::getScript(std::string_view tag) noexcept {
    const std::size_t separator = tag.find('-');
    if (separator == std::string_view::npos) {
        return {};
    }
    const std::string_view rest = tag.substr(separator + 1);
    const std::string_view subtag = rest.substr(0, rest.find('-'));
    return subtag.size() == 4 && isLetters(subtag) ? subtag : std::string_view();
}

std::optional<std::size_t> LanguageTag::findBestMatch(std::string_view tag, std::span<const std::string> candidates) {
    for (std::string_view wanted = tag; !wanted.empty(); wanted = getParent(wanted)) {
        const auto found = std::ranges::find(candidates, wanted);
        if (found != candidates.end()) {
            return static_cast<std::size_t>(found - candidates.begin());
        }
    }

    const std::string_view language = getLanguage(tag);
    const std::string_view script = getScript(tag);
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const std::string_view candidateScript = getScript(candidates[index]);
        const bool scriptsAgree = script.empty() || candidateScript.empty() || script == candidateScript;
        if (getLanguage(candidates[index]) == language && scriptsAgree) {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace haylen::localization
