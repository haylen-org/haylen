#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace haylen::localization {

// Joins items into a list in a language, such as "apples, pears, and plums" or "red or blue".
class ListFormat final {
  public:
    enum class Type : std::uint8_t {
        And,
        Or,
    };

    [[nodiscard]] static std::string format(std::string_view language, std::span<const std::string> items, Type type = Type::And);

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Type>, 2> kTypeNames;
};

} // namespace haylen::localization
