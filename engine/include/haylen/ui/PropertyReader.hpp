#pragma once

#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Insets.hpp"
#include "haylen/ui/TextValue.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

// Reads the properties of one component and remembers the keys it used, so an unknown key or a value of the wrong type is reported with the component kind and the key.
class PropertyReader final {
  public:
    PropertyReader(const core::Json& values, std::string_view componentKind);

    // Lua turns an empty table into an empty JSON object, so a list property accepts an empty object as an empty list.
    [[nodiscard]] static bool isList(const core::Json& value) noexcept;

    [[nodiscard]] bool has(std::string_view key) const;
    [[nodiscard]] const core::Json* take(std::string_view key);

    void read(std::string_view key, bool& out);
    void read(std::string_view key, std::string& out);
    void read(std::string_view key, float& out, float minimum = std::numeric_limits<float>::lowest(), float maximum = std::numeric_limits<float>::max());
    void read(std::string_view key, double& out, double minimum = std::numeric_limits<double>::lowest(), double maximum = std::numeric_limits<double>::max());
    void read(std::string_view key, int& out, int minimum = std::numeric_limits<int>::min(), int maximum = std::numeric_limits<int>::max());
    void read(std::string_view key, math::Color& out);
    void read(std::string_view key, TextValue& out);
    void read(std::string_view key, math::Insets& out);
    void read(std::string_view key, std::optional<Theme::Color>& out);

    // Reads a string that must be one of the names, storing the matching value.
    template <typename T> void readChoice(std::string_view key, T& out, std::span<const std::pair<std::string_view, T>> choices) {
        const core::Json* value = take(key);
        if (value == nullptr) {
            return;
        }
        if (value->is_string()) {
            for (const auto& [name, choice] : choices) {
                if (value->get_ref<const std::string&>() == name) {
                    out = choice;
                    return;
                }
            }
        }

        std::string names;
        for (std::size_t index = 0; index < choices.size(); ++index) {
            names += index == 0 ? "" : (index + 1 == choices.size() ? " or " : ", ");
            names += "\"" + std::string(choices[index].first) + "\"";
        }
        fail(key, "must be " + names);
    }

    // A length is a non-negative number or `auto`, which lets the content decide.
    void readLength(std::string_view key, std::optional<float>& out);

    // Throws for the first property no read asked for.
    void finish() const;

    [[noreturn]] void fail(std::string_view key, std::string_view problem) const;

    // Names a property as `kind.key`, such as `list.items`, which `describeProperty` turns into the start of a message.
    [[nodiscard]] std::string getQualifiedName(std::string_view key) const;

    // Starts a message about the property at a path such as `button.style.padding` with `The property "style.padding" of a "button"`.
    [[nodiscard]] static std::string describeProperty(std::string_view path);

    // Names a component kind with its article, such as `a "button"` or `an "image"`.
    [[nodiscard]] static std::string describeKind(std::string_view componentKind);

  private:
    template <typename Number> [[nodiscard]] static std::string describeRange(Number minimum, Number maximum);

    // Keys the GUI reads itself, so components never see them as unknown properties.
    static constexpr std::array<std::string_view, 3> kStructuralKeys{"kind", "id", "children"};

    const core::Json& properties;
    std::string kind;
    std::set<std::string, std::less<>> used;
};

} // namespace haylen::ui
