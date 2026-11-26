#include "3d/gltf/FormatNames.hpp"

#include <algorithm>
#include <limits>

namespace haylen::gltf {

const std::array<std::pair<std::uint64_t, Accessor::ComponentType>, 6> FormatNames::kComponentTypes{{
    {5120, Accessor::ComponentType::Byte},
    {5121, Accessor::ComponentType::UnsignedByte},
    {5122, Accessor::ComponentType::Short},
    {5123, Accessor::ComponentType::UnsignedShort},
    {5125, Accessor::ComponentType::UnsignedInt},
    {5126, Accessor::ComponentType::Float},
}};

const std::array<std::pair<std::string_view, Accessor::Type>, 7> FormatNames::kAccessorTypes{{
    {"SCALAR", Accessor::Type::Scalar},
    {"VEC2", Accessor::Type::Vec2},
    {"VEC3", Accessor::Type::Vec3},
    {"VEC4", Accessor::Type::Vec4},
    {"MAT2", Accessor::Type::Mat2},
    {"MAT3", Accessor::Type::Mat3},
    {"MAT4", Accessor::Type::Mat4},
}};

const std::array<std::pair<std::uint64_t, Sampler::Filter>, 6> FormatNames::kFilters{{
    {9728, Sampler::Filter::Nearest},
    {9729, Sampler::Filter::Linear},
    {9984, Sampler::Filter::NearestMipmapNearest},
    {9985, Sampler::Filter::LinearMipmapNearest},
    {9986, Sampler::Filter::NearestMipmapLinear},
    {9987, Sampler::Filter::LinearMipmapLinear},
}};

const std::array<std::pair<std::uint64_t, Sampler::Wrap>, 3> FormatNames::kWraps{{
    {33071, Sampler::Wrap::ClampToEdge},
    {33648, Sampler::Wrap::MirroredRepeat},
    {10497, Sampler::Wrap::Repeat},
}};

const std::array<std::pair<std::string_view, Material::AlphaMode>, 3> FormatNames::kAlphaModes{{
    {"OPAQUE", Material::AlphaMode::Opaque},
    {"MASK", Material::AlphaMode::Mask},
    {"BLEND", Material::AlphaMode::Blend},
}};

const std::array<std::pair<std::string_view, Camera::Type>, 2> FormatNames::kCameraTypes{{
    {"perspective", Camera::Type::Perspective},
    {"orthographic", Camera::Type::Orthographic},
}};

const std::array<std::pair<std::string_view, Animation::Interpolation>, 3> FormatNames::kInterpolations{{
    {"STEP", Animation::Interpolation::Step},
    {"LINEAR", Animation::Interpolation::Linear},
    {"CUBICSPLINE", Animation::Interpolation::CubicSpline},
}};

const std::array<std::pair<std::string_view, Animation::Path>, 4> FormatNames::kPaths{{
    {"translation", Animation::Path::Translation},
    {"rotation", Animation::Path::Rotation},
    {"scale", Animation::Path::Scale},
    {"weights", Animation::Path::Weights},
}};

// The media types of images that only an extension enables, with that extension.
const std::array<std::pair<std::string_view, std::string_view>, 3> FormatNames::kImageExtensions{{
    {"image/webp", "EXT_texture_webp"},
    {"image/avif", "EXT_texture_avif"},
    {"image/ktx2", "KHR_texture_basisu"},
}};

std::optional<Accessor::ComponentType> FormatNames::findComponentType(std::uint64_t value) noexcept {
    return lookUp(kComponentTypes, value);
}

std::optional<Accessor::Type> FormatNames::findAccessorType(std::string_view name) noexcept {
    return lookUp(kAccessorTypes, name);
}

std::optional<BufferView::Target> FormatNames::findTarget(std::uint64_t value) noexcept {
    if (value == 34962) {
        return BufferView::Target::ArrayBuffer;
    }

    if (value == 34963) {
        return BufferView::Target::ElementArrayBuffer;
    }

    return std::nullopt;
}

std::optional<Sampler::Filter> FormatNames::findFilter(std::uint64_t value) noexcept {
    return lookUp(kFilters, value);
}

std::optional<Sampler::Wrap> FormatNames::findWrap(std::uint64_t value) noexcept {
    return lookUp(kWraps, value);
}

std::optional<Material::AlphaMode> FormatNames::findAlphaMode(std::string_view name) noexcept {
    return lookUp(kAlphaModes, name);
}

std::optional<Camera::Type> FormatNames::findCameraType(std::string_view name) noexcept {
    return lookUp(kCameraTypes, name);
}

std::optional<Animation::Interpolation> FormatNames::findInterpolation(std::string_view name) noexcept {
    return lookUp(kInterpolations, name);
}

std::optional<Animation::Path> FormatNames::findPath(std::string_view name) noexcept {
    return lookUp(kPaths, name);
}

std::string FormatNames::describeFormat(Accessor::Type type, Accessor::ComponentType componentType, bool normalized) {
    std::string_view typeName;
    for (const auto& [name, value] : kAccessorTypes) {
        if (value == type) {
            typeName = name;
        }
    }

    std::string_view component;
    switch (componentType) {
    case Accessor::ComponentType::Byte:
        component = normalized ? "normalized bytes" : "bytes";
        break;
    case Accessor::ComponentType::UnsignedByte:
        component = normalized ? "normalized unsigned bytes" : "unsigned bytes";
        break;
    case Accessor::ComponentType::Short:
        component = normalized ? "normalized shorts" : "shorts";
        break;
    case Accessor::ComponentType::UnsignedShort:
        component = normalized ? "normalized unsigned shorts" : "unsigned shorts";
        break;
    case Accessor::ComponentType::UnsignedInt:
        component = "unsigned integers";
        break;
    case Accessor::ComponentType::Float:
        component = "floats";
        break;
    }

    return "\"" + std::string(typeName) + "\" of " + std::string(component);
}

double FormatNames::getLowest(Accessor::ComponentType componentType) noexcept {
    switch (componentType) {
    case Accessor::ComponentType::Byte:
        return std::numeric_limits<std::int8_t>::lowest();
    case Accessor::ComponentType::Short:
        return std::numeric_limits<std::int16_t>::lowest();
    case Accessor::ComponentType::Float:
        return std::numeric_limits<float>::lowest();
    case Accessor::ComponentType::UnsignedByte:
    case Accessor::ComponentType::UnsignedShort:
    case Accessor::ComponentType::UnsignedInt:
        return 0.0;
    }
    return 0.0;
}

double FormatNames::getHighest(Accessor::ComponentType componentType) noexcept {
    switch (componentType) {
    case Accessor::ComponentType::Byte:
        return std::numeric_limits<std::int8_t>::max();
    case Accessor::ComponentType::UnsignedByte:
        return std::numeric_limits<std::uint8_t>::max();
    case Accessor::ComponentType::Short:
        return std::numeric_limits<std::int16_t>::max();
    case Accessor::ComponentType::UnsignedShort:
        return std::numeric_limits<std::uint16_t>::max();
    case Accessor::ComponentType::UnsignedInt:
        return std::numeric_limits<std::uint32_t>::max();
    case Accessor::ComponentType::Float:
        return std::numeric_limits<float>::max();
    }
    return 0.0;
}

bool FormatNames::isImageTypeEnabled(std::string_view mimeType, const std::vector<std::string>& extensionsUsed) {
    if (mimeType == "image/png" || mimeType == "image/jpeg") {
        return true;
    }

    for (const auto& [type, extension] : kImageExtensions) {
        if (type == mimeType) {
            return std::ranges::find(extensionsUsed, extension) != extensionsUsed.end();
        }
    }

    // A media type that no extension of glTF 2.0 defines belongs to an extension the reader does not know, which the file may use without requiring it.
    return true;
}

} // namespace haylen::gltf
