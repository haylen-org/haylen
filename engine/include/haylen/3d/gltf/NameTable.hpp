#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>

namespace haylen::gltf {

// Looks up the names of the enums of glTF in the one table of names of each enum, which Lua and the description of a document share.
class NameTable final {
  public:
    template <typename T, std::size_t Size> using Entries = std::array<std::pair<std::string_view, T>, Size>;

    template <typename T, std::size_t Size> [[nodiscard]] static std::optional<T> find(const Entries<T, Size>& entries, std::string_view name) {
        const auto found = std::ranges::find(entries, name, &std::pair<std::string_view, T>::first);
        return found != entries.end() ? std::optional(found->second) : std::nullopt;
    }

    template <typename T, std::size_t Size> [[nodiscard]] static std::string_view name(const Entries<T, Size>& entries, T value) {
        return std::ranges::find(entries, value, &std::pair<std::string_view, T>::second)->first;
    }
};

} // namespace haylen::gltf
