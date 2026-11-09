#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace haylen::localization {

// Reads language tags in the forms that platforms, files and people write them, such as `pt_br`, `PT-BR` and `pt-BR`, and gives the canonical form of BCP 47: the language in lowercase, the script with a capital, the region in uppercase and the variants in lowercase, as in `zh-Hans-CN`. Extensions and private use after a subtag of one letter, such as the `-u-mu-celsius` of a regional preference, name no language and are left out.
class LanguageTag final {
  public:
    // Returns the canonical form of the tag, or nothing when the text is no language tag.
    [[nodiscard]] static std::optional<std::string> parse(std::string_view tag);

    // Returns the canonical form of the tag, and throws `std::invalid_argument` when the text is no language tag.
    [[nodiscard]] static std::string normalize(std::string_view tag);

    // Returns the canonical tag without its last subtag, such as `zh-Hans` for `zh-Hans-CN`, and an empty text for a language alone.
    [[nodiscard]] static std::string_view getParent(std::string_view tag) noexcept;

    // Finds among canonical tags the one closest to a canonical tag: the tag itself, then each of its parents, then the first of the same language whose script agrees, where a tag without a script agrees with every script.
    [[nodiscard]] static std::optional<std::size_t> findBestMatch(std::string_view tag, std::span<const std::string> candidates);

  private:
    enum class Part : std::uint8_t {
        Script,
        Region,
        Variant,
    };

    [[nodiscard]] static bool isLetters(std::string_view subtag) noexcept;
    [[nodiscard]] static bool isDigits(std::string_view subtag) noexcept;
    [[nodiscard]] static bool isAlphanumeric(std::string_view subtag) noexcept;
    [[nodiscard]] static bool isRegion(std::string_view subtag) noexcept;
    [[nodiscard]] static bool isVariant(std::string_view subtag) noexcept;
    static void toLower(std::string& text) noexcept;
    static void toUpper(std::string& text) noexcept;
    [[nodiscard]] static std::string_view getLanguage(std::string_view tag) noexcept;
    [[nodiscard]] static std::string_view getScript(std::string_view tag) noexcept;
};

} // namespace haylen::localization
