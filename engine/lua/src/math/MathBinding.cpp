#include "math/MathBinding.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "core/FloatBufferBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Promise.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/EasingCurve.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "math/ChoiceBinding.hpp"
#include "math/Geometry3Binding.hpp"
#include "math/MatrixBinding.hpp"
#include "math/PolygonBinding.hpp"
#include "math/QuatBinding.hpp"
#include "math/RaycastBinding.hpp"
#include "math/SignalFilterBinding.hpp"
#include "math/SplineBinding.hpp"
#include "math/SpringBinding.hpp"
#include "math/VectorBinding.hpp"

namespace haylen::lua::math {

// Constructors take their components as numbers, or one whole value in any form that parameters of the type accept.
bool MathBinding::isWholeValue(lua_State* L) {
    return !lua_isnoneornil(L, 1) && lua_type(L, 1) != LUA_TNUMBER;
}

int MathBinding::vec2New(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Vec2{lua::Converter<float>::readOr(L, 1, 0.0), lua::Converter<float>::readOr(L, 2, 0.0)});
    return 1;
}

haylen::math::Vec2 MathBinding::vec2FromAngle(float radians, std::optional<float> magnitude) {
    return haylen::math::Vec2::fromAngle(radians, magnitude.value_or(1.0F));
}

int MathBinding::vec2Add(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) + lua::Stack::read<haylen::math::Vec2>(L, 2));
    return 1;
}

int MathBinding::vec2Subtract(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) - lua::Stack::read<haylen::math::Vec2>(L, 2));
    return 1;
}

int MathBinding::vec2Multiply(lua_State* L) {
    if (lua_type(L, 1) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 2) * lua::Stack::read<float>(L, 1));
    } else if (lua_type(L, 2) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) * lua::Stack::read<float>(L, 2));
    } else {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) * lua::Stack::read<haylen::math::Vec2>(L, 2));
    }

    return 1;
}

int MathBinding::vec2Divide(lua_State* L) {
    if (lua_type(L, 2) == LUA_TNUMBER) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) / lua::Stack::read<float>(L, 2));
    } else {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Vec2>(L, 1) / lua::Stack::read<haylen::math::Vec2>(L, 2));
    }

    return 1;
}

int MathBinding::vec2Negate(lua_State* L) {
    lua::Stack::push(L, -lua::Stack::read<haylen::math::Vec2>(L, 1));
    return 1;
}

int MathBinding::vec2Equal(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Vec2>(L, 1) == lua::Stack::read<haylen::math::Vec2>(L, 2) ? 1 : 0);
    return 1;
}

int MathBinding::vec2ToString(lua_State* L) {
    const haylen::math::Vec2 value = lua::Stack::read<haylen::math::Vec2>(L, 1);
    lua_pushfstring(L, "Vec2(%f, %f)", static_cast<double>(value.x), static_cast<double>(value.y));
    return 1;
}

int MathBinding::vec2Unpack(lua_State* L) {
    const haylen::math::Vec2 value = lua::Stack::read<haylen::math::Vec2>(L, 1);
    lua_pushnumber(L, value.x);
    lua_pushnumber(L, value.y);
    return 2;
}

haylen::math::Vec2 MathBinding::vec2Normalized(const haylen::math::Vec2& value) {
    return value.getNormalized();
}

int MathBinding::rectNew(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Rect>(L, 1));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Rect{lua::Converter<float>::readOr(L, 1, 0.0), lua::Converter<float>::readOr(L, 2, 0.0), lua::Converter<float>::readOr(L, 3, 0.0), lua::Converter<float>::readOr(L, 4, 0.0)});
    return 1;
}

int MathBinding::rectEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Rect>(L, 1) == lua::Stack::read<haylen::math::Rect>(L, 2) ? 1 : 0);
    return 1;
}

int MathBinding::rectToString(lua_State* L) {
    const haylen::math::Rect value = lua::Stack::read<haylen::math::Rect>(L, 1);
    lua_pushfstring(L, "Rect(%f, %f, %f, %f)", static_cast<double>(value.x), static_cast<double>(value.y), static_cast<double>(value.width), static_cast<double>(value.height));
    return 1;
}

// Accepts either a point or a rectangle, telling tables apart by their `width` field.
int MathBinding::rectContains(lua_State* L) {
    const haylen::math::Rect& self = lua::Userdata::check<haylen::math::Rect>(L, 1);
    bool rectangle = lua::Userdata::test<haylen::math::Rect>(L, 2) != nullptr;
    if (!rectangle && lua::Userdata::test<haylen::math::Vec2>(L, 2) == nullptr) {
        luaL_checktype(L, 2, LUA_TTABLE);
        rectangle = lua_getfield(L, 2, "width") != LUA_TNIL;
        lua_pop(L, 1);
    }

    lua_pushboolean(L, (rectangle ? self.contains(lua::Stack::read<haylen::math::Rect>(L, 2)) : self.contains(lua::Stack::read<haylen::math::Vec2>(L, 2))) ? 1 : 0);
    return 1;
}

bool MathBinding::rectIntersects(const haylen::math::Rect& self, haylen::math::Rect other) {
    return self.intersects(other);
}

haylen::math::Rect MathBinding::rectIntersection(const haylen::math::Rect& self, haylen::math::Rect other) {
    return self.intersection(other);
}

haylen::math::Rect MathBinding::rectMerged(const haylen::math::Rect& self, haylen::math::Rect other) {
    return self.merged(other);
}

haylen::math::Rect MathBinding::rectExpanded(const haylen::math::Rect& self, float amount) {
    return self.expanded(amount);
}

int MathBinding::colorNew(lua_State* L) {
    if (isWholeValue(L)) {
        lua::Stack::push(L, lua::Stack::read<haylen::math::Color>(L, 1));
        return 1;
    }

    lua::Stack::push(L, haylen::math::Color{lua::Converter<float>::readOr(L, 1, 1.0), lua::Converter<float>::readOr(L, 2, 1.0), lua::Converter<float>::readOr(L, 3, 1.0), lua::Converter<float>::readOr(L, 4, 1.0)});
    return 1;
}

int MathBinding::colorEqual(lua_State* L) {
    lua_pushboolean(L, lua::Stack::read<haylen::math::Color>(L, 1) == lua::Stack::read<haylen::math::Color>(L, 2) ? 1 : 0);
    return 1;
}

int MathBinding::colorMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Color>(L, 1) * lua::Stack::read<haylen::math::Color>(L, 2));
    return 1;
}

int MathBinding::colorToHex(lua_State* L) {
    lua::Stack::push(L, lua::Stack::read<haylen::math::Color>(L, 1).toHex());
    return 1;
}

haylen::math::Color MathBinding::colorWithAlpha(const haylen::math::Color& self, float alpha) {
    return self.withAlpha(alpha);
}

haylen::math::Color MathBinding::colorFromHsv(float hue, float saturation, float value, std::optional<float> alpha) {
    return haylen::math::Color::fromHsv(hue, saturation, value, alpha.value_or(1.0F));
}

int MathBinding::colorToHsv(lua_State* L) {
    const haylen::math::Color::Hsv hsv = lua::Userdata::check<haylen::math::Color>(L, 1).toHsv();
    lua::Stack::push(L, hsv.hue);
    lua::Stack::push(L, hsv.saturation);
    lua::Stack::push(L, hsv.value);
    lua::Stack::push(L, hsv.alpha);
    return 4;
}

float MathBinding::ease(const haylen::math::EasingCurve& curve, float t) {
    return curve.apply(t);
}

// An `EasingCurve` value is called like the function it stands for, with the progress after the value itself.
int MathBinding::curveCall(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::EasingCurve>(L, 1).apply(lua::Stack::read<float>(L, 2)));
    return 1;
}

std::uint8_t MathBinding::checkChannel(lua_State* L, int index) {
    const lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= 0 && value <= 255, index, "expected an integer from 0 to 255");
    return static_cast<std::uint8_t>(value);
}

int MathBinding::colorFromRgba8(lua_State* L) {
    const std::uint8_t red = checkChannel(L, 1);
    const std::uint8_t green = checkChannel(L, 2);
    const std::uint8_t blue = checkChannel(L, 3);
    const std::uint8_t alpha = lua_isnoneornil(L, 4) ? 255 : checkChannel(L, 4);
    lua::Stack::push(L, haylen::math::Color::fromRgba8(red, green, blue, alpha));
    return 1;
}

int MathBinding::colorFromHex(lua_State* L) {
    const lua_Integer value = luaL_checkinteger(L, 1);
    luaL_argcheck(L, value >= 0 && value <= 0xFFFFFFFF, 1, "expected an integer from 0 to 0xFFFFFFFF");
    lua::Stack::push(L, haylen::math::Color::fromHex(static_cast<std::uint32_t>(value)));
    return 1;
}

haylen::math::Transform2D MathBinding::transformCompose(haylen::math::Vec2 position, std::optional<float> rotation, std::optional<haylen::math::Vec2> scale, std::optional<haylen::math::Vec2> skew) {
    return haylen::math::Transform2D::compose(position, rotation.value_or(0.0F), scale.value_or(haylen::math::Vec2{1.0F, 1.0F}), skew.value_or(haylen::math::Vec2{}));
}

int MathBinding::transformMultiply(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::math::Transform2D>(L, 1) * lua::Userdata::check<haylen::math::Transform2D>(L, 2));
    return 1;
}

haylen::math::Vec2 MathBinding::transformApply(const haylen::math::Transform2D& self, haylen::math::Vec2 point) {
    return self.apply(point);
}

haylen::math::Vec2 MathBinding::transformApplyVector(const haylen::math::Transform2D& self, haylen::math::Vec2 vector) {
    return self.applyVector(vector);
}

haylen::math::Transform2D MathBinding::transformInverse(const haylen::math::Transform2D& self) {
    return self.getInverse();
}

int MathBinding::randomNew(lua_State* L) {
    if (lua_isnoneornil(L, 1)) {
        lua::Userdata::emplace<haylen::math::Random>(L);
        return 1;
    }

    lua::Userdata::emplace<haylen::math::Random>(L, static_cast<std::uint64_t>(luaL_checkinteger(L, 1)));
    return 1;
}

float MathBinding::randomNextFloat(haylen::math::Random& self) {
    return self.nextFloat();
}

float MathBinding::randomRange(haylen::math::Random& self, float minimum, float maximum) {
    return self.range(minimum, maximum);
}

int MathBinding::randomInteger(haylen::math::Random& self, int minimum, int maximum) {
    return self.range(minimum, maximum);
}

bool MathBinding::randomChance(haylen::math::Random& self, float probability) {
    return self.chance(probability);
}

void MathBinding::randomReseed(haylen::math::Random& self, lua_Integer seed) {
    self.reseed(static_cast<std::uint64_t>(seed));
}

int MathBinding::randomWeightedIndex(lua_State* L) {
    haylen::math::Random& self = lua::Userdata::check<haylen::math::Random>(L, 1);
    const std::vector<float> weights = lua::Stack::read<std::vector<float>>(L, 2);
    if (weights.empty()) {
        return luaL_argerror(L, 2, "expected at least one weight");
    }

    lua_pushinteger(L, static_cast<lua_Integer>(self.weightedIndex(weights) + 1));
    return 1;
}

int MathBinding::randomShuffle(lua_State* L) {
    haylen::math::Random& self = lua::Userdata::check<haylen::math::Random>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const lua_Integer length = static_cast<lua_Integer>(lua_rawlen(L, 2));
    for (lua_Integer index = length; index > 1; --index) {
        const auto other = static_cast<lua_Integer>(self.nextU64() % static_cast<std::uint64_t>(index)) + 1;
        lua_rawgeti(L, 2, index);
        lua_rawgeti(L, 2, other);
        lua_rawseti(L, 2, index);
        lua_rawseti(L, 2, other);
    }

    lua_settop(L, 2);
    return 1;
}

// Returns the state as a string of 32 bytes, the four words of the generator in little-endian order, so it compares, stores and travels like any string.
int MathBinding::randomState(lua_State* L) {
    const std::array<std::uint64_t, 4>& state = lua::Userdata::check<haylen::math::Random>(L, 1).getState();
    std::array<char, kRandomStateSize> bytes{};
    for (std::size_t word = 0; word < state.size(); ++word) {
        for (std::size_t byte = 0; byte < 8; ++byte) {
            bytes[word * 8 + byte] = static_cast<char>((state[word] >> (byte * 8)) & 0xFFU);
        }
    }

    lua_pushlstring(L, bytes.data(), bytes.size());
    return 1;
}

int MathBinding::randomSetState(lua_State* L) {
    haylen::math::Random& self = lua::Userdata::check<haylen::math::Random>(L, 1);
    const std::string_view bytes = lua::Stack::read<std::string_view>(L, 2);
    if (bytes.size() != kRandomStateSize) {
        return luaL_error(L, "A random state needs %d bytes.", static_cast<int>(kRandomStateSize));
    }

    std::array<std::uint64_t, 4> state{};
    for (std::size_t word = 0; word < state.size(); ++word) {
        for (std::size_t byte = 0; byte < 8; ++byte) {
            state[word] |= static_cast<std::uint64_t>(static_cast<unsigned char>(bytes[word * 8 + byte])) << (byte * 8);
        }
    }

    self.setState(state);
    return 0;
}

int MathBinding::noiseNew(lua_State* L) {
    lua::Userdata::emplace<haylen::math::Noise2D>(L, static_cast<std::uint64_t>(luaL_optinteger(L, 1, 0)));
    return 1;
}

float MathBinding::noisePerlin(haylen::math::Noise2D& self, float x, float y) {
    return self.perlin(x, y);
}

float MathBinding::noiseSimplex(haylen::math::Noise2D& self, float x, float y) {
    return self.simplex(x, y);
}

float MathBinding::noiseFractal(haylen::math::Noise2D& self, float x, float y, std::optional<int> octaves, std::optional<float> lacunarity, std::optional<float> gain) {
    return self.fractal(x, y, checkOctaves(octaves.value_or(4)), lacunarity.value_or(2.0F), gain.value_or(0.5F));
}

int MathBinding::noiseWorley(lua_State* L) {
    const haylen::math::Noise2D::Cellular cellular = lua::Userdata::check<haylen::math::Noise2D>(L, 1).worley(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3));
    lua::Stack::push(L, cellular.nearest);
    lua::Stack::push(L, cellular.second);
    lua::Stack::push(L, cellular.cell);
    return 3;
}

int MathBinding::noiseWarp(lua_State* L) {
    const haylen::math::Noise2D& noise = lua::Userdata::check<haylen::math::Noise2D>(L, 1);
    const int octaves = checkOctaves(lua::Stack::read<std::optional<int>>(L, 6).value_or(3));
    const haylen::math::Vec2 warped = noise.warp(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4), lua::Converter<float>::readOr(L, 5, 1.0), octaves);
    lua::Stack::push(L, warped.x);
    lua::Stack::push(L, warped.y);
    return 2;
}

// Every octave samples the noise once more, so a bound keeps one call within a frame.
int MathBinding::checkOctaves(int octaves) {
    if (octaves < 1 || octaves > kMaxOctaves) {
        throw std::invalid_argument(std::format("The octaves must be a whole number from 1 to {}.", kMaxOctaves));
    }

    return octaves;
}

// A grid needs its columns and rows, and its other options take the defaults of `Noise2D::Grid`.
haylen::math::Noise2D::Grid MathBinding::readGrid(lua_State* L, bool intoAllowed, std::shared_ptr<haylen::core::FloatBuffer>& into) {
    luaL_checktype(L, 2, LUA_TTABLE);

    if (intoAllowed) {
        lua::Table::checkFields(L, 2, {kGridFields});
    } else {
        lua::Table::checkFields(L, 2, {std::span<const std::string_view>(kGridFields).first(kGridFields.size() - 1)});
    }

    haylen::math::Noise2D::Grid grid;
    std::string kind = "fractal";
    lua::Table::readField(L, 2, "kind", kind);
    const std::optional<haylen::math::Noise2D::Kind> named = haylen::math::Noise2D::kindFromName(kind);
    if (!named) {
        luaL_error(L, "The option \"kind\" must be \"perlin\", \"simplex\", \"fractal\" or \"worley\".");
    }

    grid.kind = *named;
    for (const char* side : {"columns", "rows"}) {
        lua_getfield(L, 2, side);
        if (lua_isnil(L, -1)) {
            luaL_error(L, "The option \"%s\" is required.", side);
        }

        if (!lua_isinteger(L, -1) || lua_tointeger(L, -1) < 1 || lua_tointeger(L, -1) > static_cast<lua_Integer>(kMaxGridSide)) {
            luaL_error(L, "The option \"%s\" must be a whole number from 1 to %d.", side, static_cast<int>(kMaxGridSide));
        }

        (side[0] == 'c' ? grid.columns : grid.rows) = static_cast<std::size_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    if (grid.columns * grid.rows > kMaxGridSamples) {
        luaL_error(L, "The grid holds %d samples, more than the limit of %d.", static_cast<int>(grid.columns * grid.rows), static_cast<int>(kMaxGridSamples));
    }

    lua::Table::readField(L, 2, "x", grid.origin.x);
    lua::Table::readField(L, 2, "y", grid.origin.y);
    lua_getfield(L, 2, "step");
    if (lua_type(L, -1) == LUA_TNUMBER) {
        const auto step = lua::Converter<float>::fromNumber(L, lua_tonumber(L, -1));
        grid.step = {step, step};
    } else if (!lua_isnil(L, -1)) {
        lua::Table::readValue(L, "step", grid.step);
    }

    lua_pop(L, 1);
    lua::Table::readField(L, 2, "octaves", grid.octaves);
    if (grid.octaves < 1 || grid.octaves > kMaxOctaves) {
        luaL_error(L, "The option \"octaves\" must be a whole number from 1 to %d.", kMaxOctaves);
    }

    lua::Table::readField(L, 2, "lacunarity", grid.lacunarity);
    lua::Table::readField(L, 2, "gain", grid.gain);
    if (intoAllowed) {
        lua_getfield(L, 2, "into");
        if (!lua_isnil(L, -1)) {
            into = lua::Userdata::checkShared<haylen::core::FloatBuffer>(L, -1);
            if (into->size() < grid.columns * grid.rows) {
                luaL_error(L, "The buffer of \"into\" holds %d floats, fewer than the %d samples of the grid.", static_cast<int>(into->size()), static_cast<int>(grid.columns * grid.rows));
            }
        }

        lua_pop(L, 1);
    }

    return grid;
}

// Samples a grid on the workers of the task pool, a band of rows each, and returns the buffer of `into` or a new one.
int MathBinding::noiseSampleGrid(lua_State* L) {
    const haylen::math::Noise2D& noise = lua::Userdata::check<haylen::math::Noise2D>(L, 1);
    std::shared_ptr<haylen::core::FloatBuffer> into;
    const haylen::math::Noise2D::Grid grid = readGrid(L, true, into);
    if (!into) {
        into = std::make_shared<haylen::core::FloatBuffer>(grid.columns * grid.rows);
    }

    const std::span<float> values = into->getValues();
    const std::size_t grain = std::max<std::size_t>(1, kGridGrain / grid.columns);
    // clang-format off
    lua::Runtime::getEngine(L).getJobs().parallelFor(0, grid.rows, grain, [&noise, &grid, values](std::size_t first, std::size_t last) {
        noise.sampleRows(grid, first, last, values);
    });
    // clang-format on
    lua::Userdata::emplace<haylen::core::FloatBuffer>(L, std::move(into));
    return 1;
}

// Samples a grid on the task pool with a copy of the noise, so the promise resolves with a new buffer while the frame goes on.
int MathBinding::noiseSampleGridAsync(lua_State* L) {
    const haylen::math::Noise2D noise = lua::Userdata::check<haylen::math::Noise2D>(L, 1);
    std::shared_ptr<haylen::core::FloatBuffer> unused;
    const haylen::math::Noise2D::Grid grid = readGrid(L, false, unused);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const lua::Promise promise(engine);
    // clang-format off
    engine.getJobs().run([noise, grid] {
        auto buffer = std::make_shared<haylen::core::FloatBuffer>(grid.columns * grid.rows);
        noise.sampleRows(grid, 0, grid.rows, buffer->getValues());
        return buffer;
    }, [promise](haylen::core::JobSystem::Result<std::shared_ptr<haylen::core::FloatBuffer>> result) {
        promise.settle(std::move(result), &pushSampledGrid);
    });
    // clang-format on
    promise.push(L);
    return 1;
}

// Every coroutine that awaits the samples receives the same buffer.
void MathBinding::pushSampledGrid(lua_State* L, const std::shared_ptr<haylen::core::FloatBuffer>& buffer) {
    lua::Userdata::emplace<haylen::core::FloatBuffer>(L, buffer);
}

int MathBinding::clampValue(lua_State* L) {
    const auto value = lua::Stack::read<float>(L, 1);
    const auto minimum = lua::Stack::read<float>(L, 2);
    const auto maximum = lua::Stack::read<float>(L, 3);
    luaL_argcheck(L, minimum <= maximum, 3, "expected a maximum of at least the minimum");
    lua::Stack::push(L, std::clamp(value, minimum, maximum));
    return 1;
}

bool MathBinding::approximatelyValue(float lhs, float rhs, std::optional<float> epsilon) {
    return haylen::math::Math::approximately(lhs, rhs, epsilon.value_or(1e-5F));
}

int MathBinding::circleIntersects(lua_State* L) {
    const auto circle = lua::Stack::read<haylen::math::Circle>(L, 1);
    lua::Stack::push(L, lua::Stack::is<haylen::math::Circle>(L, 2) ? haylen::math::Geometry::intersects(circle, lua::Stack::read<haylen::math::Circle>(L, 2)) : haylen::math::Geometry::intersects(circle, lua::Stack::read<haylen::math::Rect>(L, 2)));
    return 1;
}

haylen::math::Rect MathBinding::pointBounds(std::vector<haylen::math::Vec2> points) {
    return haylen::math::Geometry::bounds(points);
}

bool MathBinding::polygonContains(std::vector<haylen::math::Vec2> polygon, haylen::math::Vec2 point) {
    return haylen::math::Geometry::contains(polygon, point);
}

float MathBinding::polygonSignedArea(std::vector<haylen::math::Vec2> polygon) {
    return haylen::math::Geometry::signedArea(polygon);
}

haylen::math::Vec2 MathBinding::polygonCentroid(std::vector<haylen::math::Vec2> polygon) {
    return haylen::math::Geometry::centroid(polygon);
}

bool MathBinding::polygonConvex(std::vector<haylen::math::Vec2> polygon) {
    return haylen::math::Geometry::isConvex(polygon);
}

std::vector<haylen::math::Vec2> MathBinding::hull(std::vector<haylen::math::Vec2> points) {
    return haylen::math::Geometry::convexHull(points);
}

int MathBinding::triangulatePolygon(lua_State* L) {
    const std::vector<std::uint32_t> indices = haylen::math::Geometry::triangulate(PolygonBinding::readShape(L, 1));
    lua_createtable(L, static_cast<int>(indices.size()), 0);
    for (std::size_t index = 0; index < indices.size(); ++index) {
        lua_pushinteger(L, static_cast<lua_Integer>(indices[index] + 1));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Samples points with `poissonDisk({area = rect, minimumDistance = 64, maximumDistance = 128, attempts = 30, random = rng or seed = n, accept = function(point), distance = function(point)})`.
int MathBinding::poissonDisk(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kPoissonFields});
    haylen::math::PoissonDisk::Options options;
    lua::Table::readField(L, 1, "area", options.area);
    lua::Table::readField(L, 1, "minimumDistance", options.minimumDistance);
    lua::Table::readField(L, 1, "maximumDistance", options.maximumDistance);
    lua::Table::readField(L, 1, "attempts", options.attempts);

    // The generator stays on the stack while sampling, so an `accept` function that drops it from the options cannot free it.
    haylen::math::Random local(0);
    haylen::math::Random* random = &local;
    lua_getfield(L, 1, "random");
    if (!lua_isnil(L, -1)) {
        random = &lua::Userdata::check<haylen::math::Random>(L, -1);
    }

    lua_Integer seed = 0;
    lua::Table::readField(L, 1, "seed", seed);
    local.reseed(static_cast<std::uint64_t>(seed));

    lua_getfield(L, 1, "accept");
    const bool hasPredicate = lua_isfunction(L, -1);
    const int predicate = lua_gettop(L);
    if (hasPredicate) {
        // clang-format off
        options.accept = [L, predicate](haylen::math::Vec2 point) {
            lua_pushvalue(L, predicate);
            lua::Stack::push(L, point);
            lua::Runtime::protectedCall(L, 1, 1);
            const bool accepted = lua_toboolean(L, -1) != 0;
            lua_pop(L, 1);
            return accepted;
        };
        // clang-format on
    }

    lua_getfield(L, 1, "distance");
    const int spacing = lua_gettop(L);
    if (lua_isfunction(L, spacing)) {
        // clang-format off
        options.distance = [L, spacing](haylen::math::Vec2 point) {
            lua_pushvalue(L, spacing);
            lua::Stack::push(L, point);
            lua::Runtime::protectedCall(L, 1, 1);
            int isNumber = 0;
            const auto distance = lua::Converter<float>::fromNumber(L, lua_tonumberx(L, -1, &isNumber));
            if (isNumber == 0) {
                luaL_error(L, "A Poisson distance function must return a number, not %s.", luaL_typename(L, -1));
            }

            lua_pop(L, 1);
            return distance;
        };
        // clang-format on
    }

    lua::Stack::push(L, haylen::math::PoissonDisk::sample(options, *random));
    return 1;
}

void MathBinding::installClasses(lua_State* L) {
    lua::ClassBuilder<haylen::math::Vec2>(L).field<&haylen::math::Vec2::x>("x").field<&haylen::math::Vec2::y>("y").method<&haylen::math::Vec2::getLength>("length").method<&haylen::math::Vec2::getLengthSquared>("lengthSquared").method<&haylen::math::Vec2::getAngle>("angle").method<&haylen::math::Vec2::rotated>("rotated").method<&haylen::math::Vec2::getPerpendicular>("perpendicular").method<&haylen::math::Vec2::clampedLength>("clampedLength").method<&haylen::math::Vec2::isZero>("isZero").function("normalized", &lua::Binding::function<&vec2Normalized>).function("dot", &lua::Binding::function<&haylen::math::Vec2::dot>).function("cross", &lua::Binding::function<&haylen::math::Vec2::cross>).function("distance", &lua::Binding::function<&haylen::math::Vec2::distance>).function("distanceSquared", &lua::Binding::function<&haylen::math::Vec2::distanceSquared>).function("lerp", &lua::Binding::function<&haylen::math::Vec2::lerp>).function("min", &lua::Binding::function<&haylen::math::Vec2::min>).function("max", &lua::Binding::function<&haylen::math::Vec2::max>).function("floor", &lua::Binding::function<&haylen::math::Vec2::floor>).function("round", &lua::Binding::function<&haylen::math::Vec2::round>).function("unpack", &vec2Unpack).meta("__add", &vec2Add).meta("__sub", &vec2Subtract).meta("__mul", &vec2Multiply).meta("__div", &vec2Divide).meta("__unm", &vec2Negate).meta("__eq", &vec2Equal).meta("__tostring", &vec2ToString).install();

    lua::ClassBuilder<haylen::math::Rect>(L).field<&haylen::math::Rect::x>("x").field<&haylen::math::Rect::y>("y").field<&haylen::math::Rect::width>("width").field<&haylen::math::Rect::height>("height").method<&haylen::math::Rect::getLeft>("left").method<&haylen::math::Rect::getRight>("right").method<&haylen::math::Rect::getTop>("top").method<&haylen::math::Rect::getBottom>("bottom").method<&haylen::math::Rect::getPosition>("position").method<&haylen::math::Rect::getCenter>("center").method<&haylen::math::Rect::getSize>("size").method<&haylen::math::Rect::getMin>("min").method<&haylen::math::Rect::getMax>("max").method<&haylen::math::Rect::getArea>("area").method<&haylen::math::Rect::isEmpty>("empty").function("contains", &lua::Binding::native<&rectContains>).function("intersects", &lua::Binding::function<&rectIntersects>).function("intersection", &lua::Binding::function<&rectIntersection>).function("merged", &lua::Binding::function<&rectMerged>).function("expanded", &lua::Binding::function<&rectExpanded>).method<&haylen::math::Rect::inset>("inset").method<&haylen::math::Rect::translated>("translated").method<&haylen::math::Rect::clamp>("clamp").meta("__eq", &rectEqual).meta("__tostring", &rectToString).install();

    lua::ClassBuilder<haylen::math::Color>(L).field<&haylen::math::Color::r>("r").field<&haylen::math::Color::g>("g").field<&haylen::math::Color::b>("b").field<&haylen::math::Color::a>("a").function("withAlpha", &lua::Binding::function<&colorWithAlpha>).method<&haylen::math::Color::getPremultiplied>("premultiplied").function("lerp", &lua::Binding::function<&haylen::math::Color::lerp>).function("lerpHsv", &lua::Binding::function<&haylen::math::Color::lerpHsv>).function("toHsv", &colorToHsv).function("toHex", &colorToHex).method<&haylen::math::Color::getLuminance>("luminance").method<&haylen::math::Color::isDark>("dark").method<&haylen::math::Color::getContrastingColor>("contrastingColor").meta("__eq", &colorEqual).meta("__mul", &colorMultiply).meta("__tostring", &colorToHex).install();

    lua::ClassBuilder<haylen::math::Transform2D>(L).field<&haylen::math::Transform2D::a>("a").field<&haylen::math::Transform2D::b>("b").field<&haylen::math::Transform2D::c>("c").field<&haylen::math::Transform2D::d>("d").field<&haylen::math::Transform2D::tx>("tx").field<&haylen::math::Transform2D::ty>("ty").function("apply", &lua::Binding::function<&transformApply>).function("applyVector", &lua::Binding::function<&transformApplyVector>).method<&haylen::math::Transform2D::getDeterminant>("determinant").method<&haylen::math::Transform2D::getTranslation>("translation").function("inverse", &lua::Binding::function<&transformInverse>).meta("__mul", &transformMultiply).install();

    lua::ClassBuilder<haylen::math::Random>(L).function("nextFloat", &lua::Binding::function<&randomNextFloat>).function("range", &lua::Binding::function<&randomRange>).function("integer", &lua::Binding::function<&randomInteger>).function("chance", &lua::Binding::function<&randomChance>).function("reseed", &lua::Binding::function<&randomReseed>).function("weightedIndex", &lua::Binding::native<&randomWeightedIndex>).function("shuffle", &randomShuffle).function("state", &randomState).function("setState", &lua::Binding::native<&randomSetState>).install();

    lua::ClassBuilder<haylen::math::Noise2D>(L).function("perlin", &lua::Binding::function<&noisePerlin>).function("simplex", &lua::Binding::function<&noiseSimplex>).function("fractal", &lua::Binding::function<&noiseFractal>).function("worley", &lua::Binding::native<&noiseWorley>).function("warp", &lua::Binding::native<&noiseWarp>).function("sampleGrid", &lua::Binding::native<&noiseSampleGrid>).function("sampleGridAsync", &lua::Binding::native<&noiseSampleGridAsync>).install();

    lua::ClassBuilder<haylen::math::EasingCurve>(L).meta("__call", &lua::Binding::native<&curveCall>).install();

    VectorBinding::install(L);
    MatrixBinding::install(L);
    QuatBinding::install(L);
    Geometry3Binding::install(L);
    SplineBinding::install(L);
    SpringBinding::install(L);
    SignalFilterBinding::install(L);
    ChoiceBinding::install(L);
}

int MathBinding::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"vec2", &vec2New}, {"fromAngle", &lua::Binding::function<&vec2FromAngle>}, {"rect", &rectNew}, {"fromMinMax", &lua::Binding::function<&haylen::math::Rect::fromMinMax>}, {"fromCenter", &lua::Binding::function<&haylen::math::Rect::fromCenter>}, {"color", &lua::Binding::native<&colorNew>}, {"white", &lua::Binding::function<&haylen::math::Color::white>}, {"black", &lua::Binding::function<&haylen::math::Color::black>}, {"transparent", &lua::Binding::function<&haylen::math::Color::transparent>}, {"fromRgba8", &colorFromRgba8}, {"fromHex", &colorFromHex}, {"fromHsv", &lua::Binding::function<&colorFromHsv>}, {"contrast", &lua::Binding::function<&haylen::math::Color::contrast>}, {"transform", &lua::Binding::function<&transformCompose>}, {"identity", &lua::Binding::function<&haylen::math::Transform2D::identity>}, {"translation", &lua::Binding::function<&haylen::math::Transform2D::translation>}, {"rotation", &lua::Binding::function<&haylen::math::Transform2D::rotation>}, {"scaling", &lua::Binding::function<&haylen::math::Transform2D::scaling>}, {"random", &randomNew}, {"noise", &noiseNew}, {"ease", &lua::Binding::function<&ease>}, {"clamp", &clampValue}, {"lerp", &lua::Binding::function<&haylen::math::Math::lerp>}, {"inverseLerp", &lua::Binding::function<&haylen::math::Math::inverseLerp>}, {"remap", &lua::Binding::function<&haylen::math::Math::remap>}, {"smoothstep", &lua::Binding::function<&haylen::math::Math::smoothstep>}, {"moveToward", &lua::Binding::function<&haylen::math::Math::moveToward>}, {"wrapAngle", &lua::Binding::function<&haylen::math::Math::wrapAngle>}, {"dampFactor", &lua::Binding::function<&haylen::math::Math::dampFactor>}, {"radians", &lua::Binding::function<&haylen::math::Math::radians>}, {"degrees", &lua::Binding::function<&haylen::math::Math::degrees>}, {"approximately", &lua::Binding::function<&approximatelyValue>}, {"sign", &lua::Binding::function<&haylen::math::Math::sign>}, {"saturate", &lua::Binding::function<&haylen::math::Math::saturate>}, {"intersects", &circleIntersects}, {"intersection", &lua::Binding::function<&haylen::math::Geometry::intersection>}, {"closestPoint", &lua::Binding::function<&haylen::math::Geometry::closestPoint>}, {"distanceToSegment", &lua::Binding::function<&haylen::math::Geometry::distanceToSegment>}, {"bounds", &lua::Binding::function<&pointBounds>}, {"polygonContains", &lua::Binding::function<&polygonContains>}, {"polygonSignedArea", &lua::Binding::function<&polygonSignedArea>}, {"polygonCentroid", &lua::Binding::function<&polygonCentroid>}, {"polygonConvex", &lua::Binding::function<&polygonConvex>}, {"convexHull", &lua::Binding::function<&hull>}, {"triangulate", &lua::Binding::native<&triangulatePolygon>}, {"poissonDisk", &lua::Binding::native<&poissonDisk>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    VectorBinding::addFunctions(L);
    MatrixBinding::addFunctions(L);
    QuatBinding::addFunctions(L);
    Geometry3Binding::addFunctions(L);
    RaycastBinding::addFunctions(L);
    PolygonBinding::addFunctions(L);
    SplineBinding::addFunctions(L);
    SpringBinding::addFunctions(L);
    SignalFilterBinding::addFunctions(L);
    ChoiceBinding::addFunctions(L);
    lua_pushnumber(L, haylen::math::Math::kPi);
    lua_setfield(L, -2, "pi");
    lua_pushnumber(L, haylen::math::Math::kTau);
    lua_setfield(L, -2, "tau");
    lua_pushnumber(L, haylen::math::Math::kHalfPi);
    lua_setfield(L, -2, "halfPi");
    return 1;
}

void MathBinding::install(lua_State* L) {
    installClasses(L);
    lua::Binding::preload(L, "haylen.math", &open);
}

} // namespace haylen::lua::math
