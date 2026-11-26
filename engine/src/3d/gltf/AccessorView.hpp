#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <utility>

#include "haylen/3d/gltf/Document.hpp"

namespace haylen::gltf {

// Walks the elements of an accessor in order, with the values of its sparse part in place, and hands each one as the numbers it stores, so integers stay exact and normalized integers can be turned into floats by `normalize`. It checks at construction that the accessor and its buffer views lie inside the loaded buffers, and while it walks that the sparse indices grow and stay below the count, so a document that code changed never reads outside its bytes.
class AccessorView final {
  public:
    static constexpr std::size_t kMaxComponents = 16;

    // Throws `std::out_of_range` for an accessor that the document lacks and `std::invalid_argument` for one whose bytes lie outside its buffers.
    AccessorView(const Document& document, std::size_t accessor);

    [[nodiscard]] const Accessor& getAccessor() const noexcept {
        return *accessor;
    }
    [[nodiscard]] std::uint64_t getCount() const noexcept {
        return accessor->count;
    }
    [[nodiscard]] std::size_t getComponents() const noexcept {
        return components;
    }

    // The index of the element that the sparse entry at a position replaces.
    [[nodiscard]] std::uint64_t getSparseIndex(std::uint64_t position) const noexcept;

    // The float that a stored number of the component type stands for: the number itself, or for a normalized integer its value scaled into `-1` to `1` or `0` to `1`.
    [[nodiscard]] static double normalize(Accessor::ComponentType componentType, double stored) noexcept;

    // Calls `visit(element, values)` for every element, with `values` holding its components as stored.
    template <typename Visit> void forEach(Visit&& visit) const {
        switch (accessor->componentType) {
        case Accessor::ComponentType::Byte:
            walk<std::int8_t>(visit);
            break;
        case Accessor::ComponentType::UnsignedByte:
            walk<std::uint8_t>(visit);
            break;
        case Accessor::ComponentType::Short:
            walk<std::int16_t>(visit);
            break;
        case Accessor::ComponentType::UnsignedShort:
            walk<std::uint16_t>(visit);
            break;
        case Accessor::ComponentType::UnsignedInt:
            walk<std::uint32_t>(visit);
            break;
        case Accessor::ComponentType::Float:
            walk<float>(visit);
            break;
        }
    }

  private:
    // Checks that `count` items of `size` bytes, `stride` bytes apart, from an offset fit in a range of a length.
    [[nodiscard]] static bool fits(std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::uint64_t size, std::uint64_t length) noexcept;
    [[nodiscard]] const std::uint8_t* locateView(const Document& document, std::size_t view, std::uint64_t offset, std::uint64_t count, std::uint64_t stride, std::uint64_t size) const;

    template <typename T> void read(const std::uint8_t* element, std::array<double, kMaxComponents>& values) const {
        for (std::size_t component = 0; component < components; ++component) {
            T value{};
            std::memcpy(&value, element + componentOffsets[component], sizeof(T));
            values[component] = static_cast<double>(value);
        }
    }

    template <typename T, typename Visit> void walk(Visit& visit) const {
        std::array<double, kMaxComponents> values{};
        std::uint64_t position = 0;
        std::uint64_t replaced = sparseCount > 0 ? getSparseIndex(0) : accessor->count;
        std::uint64_t previous = 0;
        for (std::uint64_t element = 0; element < accessor->count; ++element) {
            if (element == replaced) {
                read<T>(sparseValues + position * elementSize, values);
                previous = replaced;
                ++position;
                replaced = position < sparseCount ? getSparseIndex(position) : accessor->count;
                if (replaced <= previous) {
                    throw std::invalid_argument("The sparse indices of an accessor do not grow from one to the next.");
                }
            } else if (base != nullptr) {
                read<T>(base + element * stride, values);
            } else {
                values.fill(0.0);
            }

            visit(element, std::as_const(values));
        }

        if (position < sparseCount) {
            throw std::invalid_argument("A sparse index of an accessor lies past its count.");
        }
    }

    const Accessor* accessor;
    std::size_t components;
    std::size_t elementSize;
    std::array<std::size_t, kMaxComponents> componentOffsets{};
    const std::uint8_t* base = nullptr;
    std::uint64_t stride = 0;
    std::uint64_t sparseCount = 0;
    const std::uint8_t* sparseIndices = nullptr;
    std::size_t sparseIndexSize = 0;
    const std::uint8_t* sparseValues = nullptr;
};

} // namespace haylen::gltf
