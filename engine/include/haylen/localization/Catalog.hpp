#pragma once

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/text/Direction.hpp"

namespace haylen::localization {

// Translated text by key in the languages the app adds. The current language follows the languages the person prefers on the system, most preferred first, until the app chooses one, and either way it is the added language closest to what is wanted, then the fallback language. Keys missing from the current language come from the fallback language, and keys missing from both come back unchanged so the gap shows on screen. Every language tag may be written with `-` or `_` and in any case, and the catalog keeps it in the canonical form of BCP 47, such as `pt-BR`.
class Catalog final {
  public:
    // The entry of a language table that declares the direction its language writes in, `leftToRight` or `rightToLeft`.
    static constexpr std::string_view kDirectionKey = "@direction";

    // Adds or merges a language table. Nested objects become dotted keys, and an object whose keys are only plural categories, `zero`, `one`, `two`, `few`, `many` and `other`, with always `other`, is a plural form. The `@direction` entry declares the direction of the language, which is left to right until a table declares otherwise. The first language added is the fallback language until the app names another.
    void add(const std::string& name, const core::Json& table);

    // Chooses the language of the app, which stays chosen while languages are added, so the current language is the added one closest to it, or the fallback language when none is close.
    void setLanguage(std::string_view value);

    // Lets the current language follow the languages of the system again, the first of them with a close added language winning.
    void followSystem();
    [[nodiscard]] bool isFollowingSystem() const noexcept {
        return choice.empty();
    }

    // Takes the languages the person prefers on the system, most preferred first, which the engine hands over whenever the system reports a change.
    void setSystemLanguages(std::span<const std::string> value);
    [[nodiscard]] const std::vector<std::string>& getSystemLanguages() const noexcept {
        return systemLanguages;
    }

    void setFallback(std::string_view value);
    [[nodiscard]] const std::string& getLanguage() const noexcept {
        return language;
    }
    [[nodiscard]] const std::string& getFallback() const noexcept {
        return fallback;
    }

    // Returns the added languages in the order they were first added.
    [[nodiscard]] const std::vector<std::string>& getLanguages() const noexcept {
        return languages;
    }

    // Returns the direction a loaded language writes in, left to right or right to left.
    [[nodiscard]] text::Direction getDirection(std::string_view name) const;

    [[nodiscard]] bool has(std::string_view key) const;

    // Replaces `{name}` placeholders with the arguments, and `{{` and `}}` with single braces. A `count` argument picks the plural form by the plural rules of the language that holds the text, an `ordinal` argument of `true` picks it by the ordinal rules, a `zero` form takes a count of 0 whatever the rules say, and a form that the entry lacks falls back to `other`.
    [[nodiscard]] std::string getText(std::string_view key, const core::Json& arguments = core::Json::object()) const;

    // Fills a text that is not in the catalog the way the catalog fills its own: `{name}` placeholders take the arguments, `{{` and `}}` give single braces, and a placeholder without an argument stays as it is.
    [[nodiscard]] static std::string format(std::string_view pattern, const core::Json& arguments);

    // Turns every text of the catalog into a pseudo translation while it is on: accented letters, a third longer and between guillemets, with the placeholders and the tags of rich text kept. Text that overflows its space, that a font cannot draw or that skipped the catalog then shows without translating the app.
    void setPseudo(bool value) noexcept {
        pseudo = value;
    }
    [[nodiscard]] bool isPseudo() const noexcept {
        return pseudo;
    }

    // Returns the added language closest to a language tag: the tag itself, then the tag without its last subtag, again and again, such as `zh-Hans` and `zh` for `zh-Hans-CN`, then the first added language of the same language whose script agrees, such as `pt-BR` for `pt-PT`.
    [[nodiscard]] std::optional<std::string> findBestMatch(std::string_view tag) const;

    // Returns the canonical form of a language tag, such as `pt-BR` for `pt_br`, and throws `std::invalid_argument` for a text that is no language tag.
    [[nodiscard]] static std::string normalizeTag(std::string_view tag);

  private:
    using Table = std::map<std::string, core::Json, std::less<>>;

    // A plural form holds only texts of plural categories, and always `other`.
    [[nodiscard]] static bool isPlural(const core::Json& value);
    static void flatten(const core::Json& table, const std::string& prefix, Table& entries);
    static void checkArguments(const core::Json& arguments);
    [[nodiscard]] static std::string formatArgument(const core::Json& value);
    [[nodiscard]] static std::string translatePseudo(std::string_view pattern);

    // Returns the entry and the language that holds it.
    [[nodiscard]] std::pair<const core::Json*, const std::string*> find(std::string_view key) const;
    void requireLanguage(std::string_view name) const;

    // Makes the current language the added language closest to the choice of the app, or to the first language of the system that has one, and the fallback language otherwise.
    void resolveLanguage();

    std::map<std::string, Table, std::less<>> tables;
    std::map<std::string, text::Direction, std::less<>> directions;
    std::vector<std::string> languages;
    std::vector<std::string> systemLanguages;
    std::string choice;
    std::string language;
    std::string fallback;
    bool pseudo = false;
};

} // namespace haylen::localization
