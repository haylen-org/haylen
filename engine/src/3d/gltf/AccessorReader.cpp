#include "haylen/3d/gltf/AccessorReader.hpp"

#include <array>
#include <stdexcept>

#include "3d/gltf/AccessorView.hpp"
#include "haylen/3d/gltf/Limits.hpp"

namespace haylen::gltf {

std::vector<float> AccessorReader::readFloats(const Document& document, std::size_t accessor) {
    const AccessorView view(document, accessor);
    const std::size_t components = view.getComponents();
    if (view.getCount() > Limits{}.maxBufferBytes / (components * sizeof(float))) {
        throw std::invalid_argument("An accessor of the glTF document gives more values than the reader holds.");
    }

    std::vector<float> values;
    values.reserve(static_cast<std::size_t>(view.getCount()) * components);
    const Accessor::ComponentType componentType = view.getAccessor().componentType;
    const bool normalized = view.getAccessor().normalized;
    // clang-format off
    view.forEach([&](std::uint64_t, const std::array<double, AccessorView::kMaxComponents>& element) {
        for (std::size_t component = 0; component < components; ++component) {
            const double value = normalized ? AccessorView::normalize(componentType, element[component]) : element[component];
            values.push_back(static_cast<float>(value));
        }
    });
    // clang-format on
    return values;
}

std::vector<std::uint32_t> AccessorReader::readUnsigned(const Document& document, std::size_t accessor) {
    const AccessorView view(document, accessor);
    const Accessor& found = view.getAccessor();
    const bool unsignedType = found.componentType == Accessor::ComponentType::UnsignedByte || found.componentType == Accessor::ComponentType::UnsignedShort || found.componentType == Accessor::ComponentType::UnsignedInt;
    if (!unsignedType || found.normalized) {
        throw std::invalid_argument("The function that reads unsigned integers takes an accessor of unsigned integers that are not normalized.");
    }

    const std::size_t components = view.getComponents();
    if (view.getCount() > Limits{}.maxBufferBytes / (components * sizeof(std::uint32_t))) {
        throw std::invalid_argument("An accessor of the glTF document gives more values than the reader holds.");
    }

    std::vector<std::uint32_t> values;
    values.reserve(static_cast<std::size_t>(view.getCount()) * components);
    // clang-format off
    view.forEach([&](std::uint64_t, const std::array<double, AccessorView::kMaxComponents>& element) {
        for (std::size_t component = 0; component < components; ++component) {
            values.push_back(static_cast<std::uint32_t>(element[component]));
        }
    });
    // clang-format on
    return values;
}

} // namespace haylen::gltf
