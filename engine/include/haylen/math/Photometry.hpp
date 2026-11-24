#pragma once

namespace haylen::math {

// The physical units of light, which lights and emissive surfaces take so a scene lit by the sun and lamps keeps the brightness of the real world. Lumens measure all the light a lamp gives, candelas the light it sends in one direction, lux the light that falls on a surface and nits, candelas per square meter, the light that leaves a surface such as a screen or a glowing sign. The exposure of a camera turns that light into the values that a screen shows, so an emissive surface of a given number of nits draws with that number times the exposure.
class Photometry final {
  public:
    // Returns the candelas of a point light that gives `lumens` evenly in every direction.
    [[nodiscard]] static float pointIntensity(float lumens) noexcept;

    // Returns the candelas of a spot light that gives `lumens` evenly into a cone whose edge lies `outerAngle` radians from its axis. Throws `std::invalid_argument` for an angle that is not above 0 and at most pi.
    [[nodiscard]] static float spotIntensity(float lumens, float outerAngle);

    // Returns the lux that a light of `candelas` gives a surface that faces it `distance` meters away. Throws `std::invalid_argument` for a distance that is not above 0.
    [[nodiscard]] static float illuminance(float candelas, float distance);

    // Returns the exposure value at the sensitivity of ISO 100 of a camera with the f-number `aperture`, the shutter open for `shutterTime` seconds and the sensitivity `iso`. Throws `std::invalid_argument` for a setting that is not above 0.
    [[nodiscard]] static float ev100FromCamera(float aperture, float shutterTime, float iso);

    // Returns the exposure value at ISO 100 that shows a scene whose average luminance is `nits` as middle gray. Throws `std::invalid_argument` for a luminance that is not above 0.
    [[nodiscard]] static float ev100FromLuminance(float nits);

    // Returns the factor that turns light in nits into the values that a screen shows, for the exposure value at ISO 100, so the brightest light that the camera keeps maps to 1.
    [[nodiscard]] static float exposure(float ev100) noexcept;

  private:
    // The calibration of a light meter, the constant of the reflected light meters of cameras.
    static constexpr float kMeterCalibration = 12.5F;
    // The ratio between the light that fills the sensor of a camera and the light its exposure value names.
    static constexpr float kSaturation = 1.2F;

    static void checkPositive(float value, const char* message);
};

} // namespace haylen::math
