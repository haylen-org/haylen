#pragma once

#include <lua.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/Noise2D.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {

// Installs haylen.math and the Vec2, Rect, Color, Transform, Random, Noise, Spline, Spring, ShuffleBag and WeightedChoice classes.
class MathLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 8> kPoissonFields{"area", "minimumDistance", "maximumDistance", "attempts", "random", "seed", "accept", "distance"};

    [[nodiscard]] static bool isWholeValue(lua_State* L);

    static int vec2New(lua_State* L);
    [[nodiscard]] static Vec2 vec2FromAngle(float radians, std::optional<float> magnitude);
    static int vec2Add(lua_State* L);
    static int vec2Subtract(lua_State* L);
    static int vec2Multiply(lua_State* L);
    static int vec2Divide(lua_State* L);
    static int vec2Negate(lua_State* L);
    static int vec2Equal(lua_State* L);
    static int vec2ToString(lua_State* L);
    static int vec2Unpack(lua_State* L);
    [[nodiscard]] static Vec2 vec2Normalized(const Vec2& value);

    static int rectNew(lua_State* L);
    static int rectEqual(lua_State* L);
    static int rectToString(lua_State* L);
    static int rectContains(lua_State* L);
    [[nodiscard]] static bool rectIntersects(const Rect& self, Rect other);
    [[nodiscard]] static Rect rectIntersection(const Rect& self, Rect other);
    [[nodiscard]] static Rect rectMerged(const Rect& self, Rect other);
    [[nodiscard]] static Rect rectExpanded(const Rect& self, float amount);

    static int colorNew(lua_State* L);
    static int colorEqual(lua_State* L);
    static int colorMultiply(lua_State* L);
    static int colorToHex(lua_State* L);
    [[nodiscard]] static Color colorWithAlpha(const Color& self, float alpha);
    [[nodiscard]] static Color colorFromHsv(float hue, float saturation, float value, std::optional<float> alpha);
    static int colorToHsv(lua_State* L);
    [[nodiscard]] static float ease(const EasingCurve& curve, float t);
    [[nodiscard]] static std::uint8_t checkChannel(lua_State* L, int index);
    static int colorFromRgba8(lua_State* L);
    static int colorFromHex(lua_State* L);

    [[nodiscard]] static Transform2D transformCompose(Vec2 position, std::optional<float> rotation, std::optional<Vec2> scale, std::optional<Vec2> skew);
    static int transformMultiply(lua_State* L);
    [[nodiscard]] static Vec2 transformApply(const Transform2D& self, Vec2 point);
    [[nodiscard]] static Vec2 transformApplyVector(const Transform2D& self, Vec2 vector);
    [[nodiscard]] static Transform2D transformInverse(const Transform2D& self);

    static int randomNew(lua_State* L);
    [[nodiscard]] static float randomFloat(Random& self);
    [[nodiscard]] static float randomRange(Random& self, float minimum, float maximum);
    [[nodiscard]] static int randomInteger(Random& self, int minimum, int maximum);
    [[nodiscard]] static bool randomChance(Random& self, float probability);
    static void randomReseed(Random& self, lua_Integer seed);
    static int randomPick(lua_State* L);
    static int randomShuffle(lua_State* L);

    static int noiseNew(lua_State* L);
    [[nodiscard]] static float noisePerlin(Noise2D& self, float x, float y);
    [[nodiscard]] static float noiseSimplex(Noise2D& self, float x, float y);
    [[nodiscard]] static float noiseFractal(Noise2D& self, float x, float y, std::optional<int> octaves, std::optional<float> lacunarity, std::optional<float> gain);
    static int noiseWorley(lua_State* L);
    static int noiseWarp(lua_State* L);

    [[nodiscard]] static float clampValue(float value, float minimum, float maximum);
    [[nodiscard]] static bool approximatelyValue(float lhs, float rhs, std::optional<float> epsilon);

    static int circleIntersects(lua_State* L);
    [[nodiscard]] static Rect pointBounds(std::vector<Vec2> points);
    [[nodiscard]] static bool polygonContains(std::vector<Vec2> polygon, Vec2 point);
    [[nodiscard]] static float polygonArea(std::vector<Vec2> polygon);
    [[nodiscard]] static Vec2 polygonCentroid(std::vector<Vec2> polygon);
    [[nodiscard]] static bool polygonConvex(std::vector<Vec2> polygon);
    [[nodiscard]] static std::vector<Vec2> hull(std::vector<Vec2> points);
    static int triangulatePolygon(lua_State* L);
    static int poissonDisk(lua_State* L);

    static void installClasses(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::math
