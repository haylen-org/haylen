#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::localization {

// Translated text by key. Keys missing from the current language come from the fallback language, and keys missing from both come back unchanged so the gap shows on screen. The first language added becomes the current and fallback language until others are chosen.
class Catalog final {
  public:
    // Adds or merges a language table. Nested objects become dotted keys, and an object with only zero, one and other texts, and always other, is a plural form.
    void add(const std::string& name, const core::Json& table);

    void setLanguage(std::string_view value);
    void setFallback(std::string_view value);
    [[nodiscard]] const std::string& getLanguage() const noexcept {
        return language;
    }
    [[nodiscard]] const std::string& getFallback() const noexcept {
        return fallback;
    }
    [[nodiscard]] std::vector<std::string> getLanguages() const;

    [[nodiscard]] bool has(std::string_view key) const;

    // Replaces {name} placeholders with the arguments, and {{ and }} with single braces. A count argument picks the plural form: zero for 0 when present, one for 1 and other for anything else.
    [[nodiscard]] std::string getText(std::string_view key, const core::Json& arguments = core::Json::object()) const;

    // Returns the loaded language that best matches a BCP 47 tag, trying the exact tag and then the same base language, as pt for pt-BR or pt-BR for pt.
    [[nodiscard]] std::optional<std::string> findBestMatch(std::string_view tag) const;

  private:
    using Table = std::map<std::string, core::Json, std::less<>>;

    // A plural form holds only zero, one and other texts, and always other.
    [[nodiscard]] static bool isPlural(const core::Json& value);
    static void flatten(const core::Json& table, const std::string& prefix, Table& entries);
    [[nodiscard]] static std::string normalizeTag(std::string_view tag);
    [[nodiscard]] static std::string_view getBaseLanguage(std::string_view tag);
    [[nodiscard]] static std::string formatArgument(const core::Json& value);
    [[nodiscard]] static std::string format(std::string_view pattern, const core::Json& arguments);

    [[nodiscard]] const core::Json* find(std::string_view key) const;
    void requireLanguage(std::string_view name) const;

    std::map<std::string, Table, std::less<>> tables;
    std::string language;
    std::string fallback;
};

} // namespace haylen::localization
