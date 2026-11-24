#include "haylen/math/Photometry.hpp"

#include <cmath>
#include <stdexcept>

#include "haylen/math/Math.hpp"

namespace haylen::math {

float Photometry::pointIntensity(float lumens) noexcept {
    return lumens / (2.0F * Math::kTau);
}

// A cone whose edge lies `outerAngle` from its axis covers a solid angle of two pi times one minus the cosine of that angle.
float Photometry::spotIntensity(float lumens, float outerAngle) {
    if (!(outerAngle > 0.0F && outerAngle <= Math::kPi)) {
        throw std::invalid_argument("The cone of a spot light needs an outer angle above 0 and at most pi radians.");
    }

    return lumens / (Math::kTau * (1.0F - std::cos(outerAngle)));
}

float Photometry::illuminance(float candelas, float distance) {
    checkPositive(distance, "The distance to a light must be above 0 meters.");
    return candelas / (distance * distance);
}

float Photometry::ev100FromCamera(float aperture, float shutterTime, float iso) {
    checkPositive(aperture, "The aperture of a camera must be an f-number above 0.");
    checkPositive(shutterTime, "The shutter time of a camera must be above 0 seconds.");
    checkPositive(iso, "The sensitivity of a camera must be an ISO above 0.");
    return std::log2(aperture * aperture / shutterTime * 100.0F / iso);
}

float Photometry::ev100FromLuminance(float nits) {
    checkPositive(nits, "The average luminance of a scene must be above 0 nits.");
    return std::log2(nits * 100.0F / kMeterCalibration);
}

float Photometry::exposure(float ev100) noexcept {
    return 1.0F / (kSaturation * std::exp2(ev100));
}

void Photometry::checkPositive(float value, const char* message) {
    if (!(value > 0.0F) || !std::isfinite(value)) {
        throw std::invalid_argument(message);
    }
}

} // namespace haylen::math
