#include "haylen/math/ColorSpace.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace haylen::math {

const std::array<std::pair<std::string_view, ColorSpace::Kind>, 3> ColorSpace::kKindNames{{{"srgb", Kind::Srgb}, {"displayP3", Kind::DisplayP3}, {"hdr10", Kind::Hdr10}}};

std::optional<ColorSpace::Kind> ColorSpace::kindFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kKindNames, name, &std::pair<std::string_view, Kind>::first);
    return found != kKindNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view ColorSpace::kindName(Kind value) noexcept {
    return std::ranges::find(kKindNames, value, &std::pair<std::string_view, Kind>::second)->first;
}

// The curve runs in double precision, so its ends land exactly on 0 and 1.
float ColorSpace::encodeSrgb(float linear) noexcept {
    const double magnitude = std::fabs(linear);
    const double encoded = magnitude <= 0.0031308 ? magnitude * 12.92 : 1.055 * std::pow(magnitude, 1.0 / 2.4) - 0.055;
    return std::copysign(static_cast<float>(encoded), linear);
}

float ColorSpace::decodeSrgb(float encoded) noexcept {
    const double magnitude = std::fabs(encoded);
    const double linear = magnitude <= 0.04045 ? magnitude / 12.92 : std::pow((magnitude + 0.055) / 1.055, 2.4);
    return std::copysign(static_cast<float>(linear), encoded);
}

float ColorSpace::encodePq(float nits) noexcept {
    const float share = std::clamp(nits / kPeakNits, 0.0F, 1.0F);
    const float lifted = std::pow(share, kPqM1);
    return std::pow((kPqC1 + kPqC2 * lifted) / (1.0F + kPqC3 * lifted), kPqM2);
}

float ColorSpace::decodePq(float signal) noexcept {
    const float lowered = std::pow(std::clamp(signal, 0.0F, 1.0F), 1.0F / kPqM2);
    const float share = std::pow(std::max(lowered - kPqC1, 0.0F) / (kPqC2 - kPqC3 * lowered), 1.0F / kPqM1);
    return share * kPeakNits;
}

Color ColorSpace::convert(const Color& linear, Kind from, Kind to) noexcept {
    if (from == to) {
        return linear;
    }

    const Color moved = fromXyz(toXyz(linear, from), to);
    return moved.withAlpha(linear.a);
}

Vec3 ColorSpace::toXyz(const Color& linear, Kind space) noexcept {
    return getToXyz(space) * Vec3{linear.r, linear.g, linear.b};
}

Color ColorSpace::fromXyz(Vec3 xyz, Kind space) noexcept {
    const Vec3 rgb = getFromXyz(space) * xyz;
    return {rgb.x, rgb.y, rgb.z, 1.0F};
}

float ColorSpace::luminance(const Color& linear, Kind space) noexcept {
    return toXyz(linear, space).y;
}

Color ColorSpace::encode(const Color& linear, Kind output, float referenceWhite) {
    checkReferenceWhite(referenceWhite);
    const Color moved = convert(linear, Kind::Srgb, output);
    if (output != Kind::Hdr10) {
        return {encodeSrgb(moved.r), encodeSrgb(moved.g), encodeSrgb(moved.b), moved.a};
    }

    return {encodePq(moved.r * referenceWhite), encodePq(moved.g * referenceWhite), encodePq(moved.b * referenceWhite), moved.a};
}

Color ColorSpace::decode(const Color& signal, Kind output, float referenceWhite) {
    checkReferenceWhite(referenceWhite);
    Color linear{decodeSrgb(signal.r), decodeSrgb(signal.g), decodeSrgb(signal.b), signal.a};
    if (output == Kind::Hdr10) {
        linear = {decodePq(signal.r) / referenceWhite, decodePq(signal.g) / referenceWhite, decodePq(signal.b) / referenceWhite, signal.a};
    }

    return convert(linear, output, Kind::Srgb);
}

const Mat3& ColorSpace::getToXyz(Kind space) noexcept {
    switch (space) {
    case Kind::Srgb:
        return kSrgbToXyz;
    case Kind::DisplayP3:
        return kDisplayP3ToXyz;
    case Kind::Hdr10:
        return kRec2020ToXyz;
    }

    return kSrgbToXyz;
}

const Mat3& ColorSpace::getFromXyz(Kind space) noexcept {
    switch (space) {
    case Kind::Srgb:
        return kXyzToSrgb;
    case Kind::DisplayP3:
        return kXyzToDisplayP3;
    case Kind::Hdr10:
        return kXyzToRec2020;
    }

    return kXyzToSrgb;
}

void ColorSpace::checkReferenceWhite(float referenceWhite) {
    if (!(referenceWhite > 0.0F && referenceWhite <= kPeakNits)) {
        throw std::invalid_argument("The reference white must be a brightness above 0 and at most 10000 nits.");
    }
}

} // namespace haylen::math
