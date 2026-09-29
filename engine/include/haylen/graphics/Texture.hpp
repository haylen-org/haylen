#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {

struct TextureResource;

// Shared handle to a GPU texture. The GPU resource is released on the frame thread after the last handle goes away.
class Texture final {
  public:
    enum class Filter : std::uint8_t {
        Nearest,
        Linear,
    };

    enum class Wrap : std::uint8_t {
        Clamp,
        Repeat,
        Mirror,
    };

    struct Options {
        Filter filter = Filter::Nearest;
        Wrap wrap = Wrap::Clamp;

        [[nodiscard]] constexpr bool operator==(const Options&) const noexcept = default;
    };

    Texture() = default;
    explicit Texture(std::shared_ptr<TextureResource> value) noexcept : resource(std::move(value)) {}

    // Resolves the filter names "nearest" and "linear" and the wrap names "clamp", "repeat" and "mirror".
    [[nodiscard]] static std::optional<Filter> filterFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view filterName(Filter value) noexcept;
    [[nodiscard]] static std::optional<Wrap> wrapFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view wrapName(Wrap value) noexcept;

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] int getWidth() const noexcept;
    [[nodiscard]] int getHeight() const noexcept;
    [[nodiscard]] math::Vec2 getSize() const noexcept;
    [[nodiscard]] std::uint32_t getId() const noexcept;
    [[nodiscard]] Options getOptions() const noexcept;
    [[nodiscard]] const std::shared_ptr<TextureResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const Texture& other) const noexcept {
        return resource == other.resource;
    }

  private:
    static const std::array<std::pair<std::string_view, Filter>, 2> kFilterNames;
    static const std::array<std::pair<std::string_view, Wrap>, 3> kWrapNames;

    std::shared_ptr<TextureResource> resource;
};

} // namespace haylen::graphics
