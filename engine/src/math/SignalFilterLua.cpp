#include "math/SignalFilterLua.hpp"

#include <cstddef>
#include <string>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/HampelFilter.hpp"
#include "haylen/math/KalmanFilter.hpp"
#include "haylen/math/LowPassFilter.hpp"
#include "haylen/math/MedianFilter.hpp"
#include "haylen/math/MovingAverage.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

template <> struct Type<math::LowPassFilter> {
    static constexpr const char* name = "haylen.LowPassFilter";
    using Storage = math::LowPassFilter;
};

template <> struct Type<math::MovingAverage> {
    static constexpr const char* name = "haylen.MovingAverage";
    using Storage = math::MovingAverage;
};

template <> struct Type<math::MedianFilter> {
    static constexpr const char* name = "haylen.MedianFilter";
    using Storage = math::MedianFilter;
};

template <> struct Type<math::HampelFilter> {
    static constexpr const char* name = "haylen.HampelFilter";
    using Storage = math::HampelFilter;
};

template <> struct Type<math::KalmanFilter> {
    static constexpr const char* name = "haylen.KalmanFilter";
    using Storage = math::KalmanFilter;
};

} // namespace haylen::lua

namespace haylen::math {

void SignalFilterLua::checkOptions(lua_State* L, std::span<const std::string_view> fields) {
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {fields});
    }
}

bool SignalFilterLua::readValues(lua_State* L, int index, int dimensions, std::span<float> values) {
    if (dimensions == 2 && lua_gettop(L) == index && lua::Stack::is<Vec2>(L, index)) {
        const Vec2 vector = lua::Stack::read<Vec2>(L, index);
        values[0] = vector.x;
        values[1] = vector.y;
        return true;
    }
    if (lua_gettop(L) - index + 1 != dimensions) {
        luaL_error(L, "The filter takes %d values, not %d.", dimensions, lua_gettop(L) - index + 1);
    }
    for (int offset = 0; offset < dimensions; ++offset) {
        values[static_cast<std::size_t>(offset)] = lua::Stack::read<float>(L, index + offset);
    }
    return false;
}

int SignalFilterLua::pushValues(lua_State* L, std::span<const float> values, bool vector) {
    if (vector) {
        lua::Stack::push(L, Vec2{values[0], values[1]});
        return 1;
    }
    for (const float value : values) {
        lua::Stack::push(L, value);
    }
    return static_cast<int>(values.size());
}

int SignalFilterLua::newLowPass(lua_State* L) {
    checkOptions(L, kLowPassFields);
    float cutoff = 5.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "cutoff", cutoff);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }
    lua::Userdata::emplace<LowPassFilter>(L, cutoff, dimensions);
    return 1;
}

int SignalFilterLua::newMovingAverage(lua_State* L) {
    checkOptions(L, kWindowFields);
    int window = 10;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }
    lua::Userdata::emplace<MovingAverage>(L, window, dimensions);
    return 1;
}

int SignalFilterLua::newMedian(lua_State* L) {
    checkOptions(L, kWindowFields);
    int window = 5;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }
    lua::Userdata::emplace<MedianFilter>(L, window, dimensions);
    return 1;
}

int SignalFilterLua::newHampel(lua_State* L) {
    checkOptions(L, kHampelFields);
    int window = 7;
    float threshold = 3.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "threshold", threshold);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }
    lua::Userdata::emplace<HampelFilter>(L, window, threshold, dimensions);
    return 1;
}

int SignalFilterLua::newKalman(lua_State* L) {
    checkOptions(L, kKalmanFields);
    float processNoise = 0.01F;
    float measurementNoise = 1.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "processNoise", processNoise);
        lua::Table::readField(L, 1, "measurementNoise", measurementNoise);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }
    lua::Userdata::emplace<KalmanFilter>(L, processNoise, measurementNoise, dimensions);
    return 1;
}

int SignalFilterLua::updateLowPass(lua_State* L) {
    LowPassFilter& filter = lua::Userdata::check<LowPassFilter>(L, 1);
    std::array<float, LowPassFilter::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 3, filter.getDimensions(), used);
    filter.update(lua::Stack::read<float>(L, 2), used);
    return pushValues(L, used, vector);
}

int SignalFilterLua::updateMovingAverage(lua_State* L) {
    MovingAverage& filter = lua::Userdata::check<MovingAverage>(L, 1);
    std::array<float, MovingAverage::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterLua::updateMedian(lua_State* L) {
    MedianFilter& filter = lua::Userdata::check<MedianFilter>(L, 1);
    std::array<float, MedianFilter::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterLua::updateHampel(lua_State* L) {
    HampelFilter& filter = lua::Userdata::check<HampelFilter>(L, 1);
    std::array<float, HampelFilter::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterLua::updateKalman(lua_State* L) {
    KalmanFilter& filter = lua::Userdata::check<KalmanFilter>(L, 1);
    std::array<float, KalmanFilter::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

void SignalFilterLua::install(lua_State* L) {
    lua::ClassBuilder<LowPassFilter>(L).function("update", &lua::Binding::native<&updateLowPass>).method<&LowPassFilter::reset>("reset").accessor<&LowPassFilter::getCutoff, &LowPassFilter::setCutoff>("cutoff").property("dimensions", &lua::Binding::method<&LowPassFilter::getDimensions>).install();
    lua::ClassBuilder<MovingAverage>(L).function("update", &lua::Binding::native<&updateMovingAverage>).method<&MovingAverage::reset>("reset").property("window", &lua::Binding::method<&MovingAverage::getWindow>).property("dimensions", &lua::Binding::method<&MovingAverage::getDimensions>).install();
    lua::ClassBuilder<MedianFilter>(L).function("update", &lua::Binding::native<&updateMedian>).method<&MedianFilter::reset>("reset").property("window", &lua::Binding::method<&MedianFilter::getWindow>).property("dimensions", &lua::Binding::method<&MedianFilter::getDimensions>).install();
    lua::ClassBuilder<HampelFilter>(L).function("update", &lua::Binding::native<&updateHampel>).method<&HampelFilter::reset>("reset").property("outlier", &lua::Binding::method<&HampelFilter::isOutlier>).property("window", &lua::Binding::method<&HampelFilter::getWindow>).property("threshold", &lua::Binding::method<&HampelFilter::getThreshold>).property("dimensions", &lua::Binding::method<&HampelFilter::getDimensions>).install();
    lua::ClassBuilder<KalmanFilter>(L).function("update", &lua::Binding::native<&updateKalman>).method<&KalmanFilter::reset>("reset").property("uncertainty", &lua::Binding::method<&KalmanFilter::getUncertainty>).property("dimensions", &lua::Binding::method<&KalmanFilter::getDimensions>).install();
}

void SignalFilterLua::addFunctions(lua_State* L) {
    for (const auto& [name, function] : {std::pair{"lowPassFilter", &lua::Binding::native<&newLowPass>}, std::pair{"movingAverage", &lua::Binding::native<&newMovingAverage>}, std::pair{"medianFilter", &lua::Binding::native<&newMedian>}, std::pair{"hampelFilter", &lua::Binding::native<&newHampel>}, std::pair{"kalmanFilter", &lua::Binding::native<&newKalman>}}) {
        lua_pushcfunction(L, function);
        lua_setfield(L, -2, name);
    }
}

} // namespace haylen::math
