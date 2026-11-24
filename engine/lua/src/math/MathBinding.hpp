#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/core/FloatBuffer.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua::math {

// Installs `haylen.math` and the `Vec2`, `Vec3`, `Vec4`, `Mat3`, `Mat4`, `Quat`, `Plane`, the 3D shapes, `Rect`, `Color`, `Transform2D`, `Transform3D`, `DualQuat`, `Random`, `Noise2D`, `Spline2`, the 3D curves and NURBS shapes, `Spring`, `ShuffleBag` and `WeightedChoice` classes.
class MathBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::size_t kRandomStateSize = 32;
    static constexpr std::array<std::string_view, 10> kGridFields{"kind", "columns", "rows", "x", "y", "step", "octaves", "lacunarity", "gain", "into"};
    static constexpr std::size_t kMaxGridSide = 16384;
    static constexpr std::size_t kMaxGridSamples = std::size_t{1} << 24U;
    static constexpr int kMaxOctaves = 16;
    static constexpr std::size_t kGridGrain = 4096;
    static constexpr std::array<std::string_view, 8> kPoissonFields{"area", "minimumDistance", "maximumDistance", "attempts", "random", "seed", "accept", "distance"};

    [[nodiscard]] static bool isWholeValue(lua_State* L);

    static int vec2New(lua_State* L);
    [[nodiscard]] static haylen::math::Vec2 vec2FromAngle(float radians, std::optional<float> magnitude);
    static int vec2Add(lua_State* L);
    static int vec2Subtract(lua_State* L);
    static int vec2Multiply(lua_State* L);
    static int vec2Divide(lua_State* L);
    static int vec2Negate(lua_State* L);
    static int vec2Equal(lua_State* L);
    static int vec2ToString(lua_State* L);
    static int vec2Unpack(lua_State* L);
    [[nodiscard]] static haylen::math::Vec2 vec2Normalized(const haylen::math::Vec2& value);

    static int rectNew(lua_State* L);
    static int rectEqual(lua_State* L);
    static int rectToString(lua_State* L);
    static int rectContains(lua_State* L);
    [[nodiscard]] static bool rectIntersects(const haylen::math::Rect& self, haylen::math::Rect other);
    [[nodiscard]] static haylen::math::Rect rectIntersection(const haylen::math::Rect& self, haylen::math::Rect other);
    [[nodiscard]] static haylen::math::Rect rectMerged(const haylen::math::Rect& self, haylen::math::Rect other);
    [[nodiscard]] static haylen::math::Rect rectExpanded(const haylen::math::Rect& self, float amount);

    static int colorNew(lua_State* L);
    static int colorEqual(lua_State* L);
    static int colorMultiply(lua_State* L);
    static int colorToHex(lua_State* L);
    [[nodiscard]] static haylen::math::Color colorWithAlpha(const haylen::math::Color& self, float alpha);
    [[nodiscard]] static haylen::math::Color colorFromHsv(float hue, float saturation, float value, std::optional<float> alpha);
    static int colorToHsv(lua_State* L);
    [[nodiscard]] static float ease(const haylen::math::EasingCurve& curve, float t);
    static int curveCall(lua_State* L);
    [[nodiscard]] static std::uint8_t checkChannel(lua_State* L, int index);
    static int colorFromRgba8(lua_State* L);
    static int colorFromHex(lua_State* L);

    [[nodiscard]] static haylen::math::Transform2D transformCompose(haylen::math::Vec2 position, std::optional<float> rotation, std::optional<haylen::math::Vec2> scale, std::optional<haylen::math::Vec2> skew);
    static int transformMultiply(lua_State* L);
    [[nodiscard]] static haylen::math::Vec2 transformApply(const haylen::math::Transform2D& self, haylen::math::Vec2 point);
    [[nodiscard]] static haylen::math::Vec2 transformApplyVector(const haylen::math::Transform2D& self, haylen::math::Vec2 vector);
    [[nodiscard]] static haylen::math::Transform2D transformInverse(const haylen::math::Transform2D& self);

    static int randomNew(lua_State* L);
    [[nodiscard]] static float randomNextFloat(haylen::math::Random& self);
    [[nodiscard]] static float randomRange(haylen::math::Random& self, float minimum, float maximum);
    [[nodiscard]] static int randomInteger(haylen::math::Random& self, int minimum, int maximum);
    [[nodiscard]] static bool randomChance(haylen::math::Random& self, float probability);
    static void randomReseed(haylen::math::Random& self, lua_Integer seed);
    static int randomWeightedIndex(lua_State* L);
    static int randomShuffle(lua_State* L);
    static int randomState(lua_State* L);
    static int randomSetState(lua_State* L);

    static int noiseNew(lua_State* L);
    [[nodiscard]] static float noisePerlin(haylen::math::Noise2D& self, float x, float y);
    [[nodiscard]] static float noiseSimplex(haylen::math::Noise2D& self, float x, float y);
    [[nodiscard]] static float noiseFractal(haylen::math::Noise2D& self, float x, float y, std::optional<int> octaves, std::optional<float> lacunarity, std::optional<float> gain);
    static int noiseWorley(lua_State* L);
    static int noiseWarp(lua_State* L);
    [[nodiscard]] static int checkOctaves(int octaves);

    // Reads the grid of `sampleGrid` from the options at index 2, and the buffer of `into` when `into` is allowed and given.
    [[nodiscard]] static haylen::math::Noise2D::Grid readGrid(lua_State* L, bool intoAllowed, std::shared_ptr<haylen::core::FloatBuffer>& into);
    static int noiseSampleGrid(lua_State* L);
    static int noiseSampleGridAsync(lua_State* L);
    static void pushSampledGrid(lua_State* L, const std::shared_ptr<haylen::core::FloatBuffer>& buffer);

    static int clampValue(lua_State* L);
    [[nodiscard]] static bool approximatelyValue(float lhs, float rhs, std::optional<float> epsilon);

    static int circleIntersects(lua_State* L);
    [[nodiscard]] static haylen::math::Rect pointBounds(std::vector<haylen::math::Vec2> points);
    [[nodiscard]] static bool polygonContains(std::vector<haylen::math::Vec2> polygon, haylen::math::Vec2 point);
    [[nodiscard]] static float polygonSignedArea(std::vector<haylen::math::Vec2> polygon);
    [[nodiscard]] static haylen::math::Vec2 polygonCentroid(std::vector<haylen::math::Vec2> polygon);
    [[nodiscard]] static bool polygonConvex(std::vector<haylen::math::Vec2> polygon);
    [[nodiscard]] static std::vector<haylen::math::Vec2> hull(std::vector<haylen::math::Vec2> points);
    static int triangulatePolygon(lua_State* L);
    static int poissonDisk(lua_State* L);

    static void installClasses(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::lua::math
