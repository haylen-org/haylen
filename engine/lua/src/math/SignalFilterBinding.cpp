#include "math/SignalFilterBinding.hpp"

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
#include "haylen/math/SignalFilterHelper.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::lua {

template <> struct Type<haylen::math::LowPassFilter> {
    static constexpr const char* name = "haylen.LowPassFilter";
    using Storage = haylen::math::LowPassFilter;
};

template <> struct Type<haylen::math::MovingAverage> {
    static constexpr const char* name = "haylen.MovingAverage";
    using Storage = haylen::math::MovingAverage;
};

template <> struct Type<haylen::math::MedianFilter> {
    static constexpr const char* name = "haylen.MedianFilter";
    using Storage = haylen::math::MedianFilter;
};

template <> struct Type<haylen::math::HampelFilter> {
    static constexpr const char* name = "haylen.HampelFilter";
    using Storage = haylen::math::HampelFilter;
};

template <> struct Type<haylen::math::KalmanFilter> {
    static constexpr const char* name = "haylen.KalmanFilter";
    using Storage = haylen::math::KalmanFilter;
};

} // namespace haylen::lua

namespace haylen::lua::math {

void SignalFilterBinding::checkOptions(lua_State* L, std::span<const std::string_view> fields) {
    if (!lua_isnoneornil(L, 1)) {
        luaL_checktype(L, 1, LUA_TTABLE);
        lua::Table::checkFields(L, 1, {fields});
    }
}

bool SignalFilterBinding::readValues(lua_State* L, int index, int dimensions, std::span<float> values) {
    if (dimensions == 2 && lua_gettop(L) == index && lua::Stack::is<haylen::math::Vec2>(L, index)) {
        const haylen::math::Vec2 vector = lua::Stack::read<haylen::math::Vec2>(L, index);
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

int SignalFilterBinding::pushValues(lua_State* L, std::span<const float> values, bool vector) {
    if (vector) {
        lua::Stack::push(L, haylen::math::Vec2{values[0], values[1]});
        return 1;
    }

    for (const float value : values) {
        lua::Stack::push(L, value);
    }

    return static_cast<int>(values.size());
}

int SignalFilterBinding::newLowPass(lua_State* L) {
    checkOptions(L, kLowPassFields);
    float cutoff = 5.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "cutoff", cutoff);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }

    lua::Userdata::emplace<haylen::math::LowPassFilter>(L, cutoff, dimensions);
    return 1;
}

int SignalFilterBinding::newMovingAverage(lua_State* L) {
    checkOptions(L, kWindowFields);
    int window = 10;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }

    lua::Userdata::emplace<haylen::math::MovingAverage>(L, window, dimensions);
    return 1;
}

int SignalFilterBinding::newMedian(lua_State* L) {
    checkOptions(L, kWindowFields);
    int window = 5;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }

    lua::Userdata::emplace<haylen::math::MedianFilter>(L, window, dimensions);
    return 1;
}

int SignalFilterBinding::newHampel(lua_State* L) {
    checkOptions(L, kHampelFields);
    int window = 7;
    float threshold = 3.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "window", window);
        lua::Table::readField(L, 1, "threshold", threshold);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }

    lua::Userdata::emplace<haylen::math::HampelFilter>(L, window, threshold, dimensions);
    return 1;
}

int SignalFilterBinding::newKalman(lua_State* L) {
    checkOptions(L, kKalmanFields);
    float processNoise = 0.01F;
    float measurementNoise = 1.0F;
    int dimensions = 1;
    if (lua_istable(L, 1)) {
        lua::Table::readField(L, 1, "processNoise", processNoise);
        lua::Table::readField(L, 1, "measurementNoise", measurementNoise);
        lua::Table::readField(L, 1, "dimensions", dimensions);
    }

    lua::Userdata::emplace<haylen::math::KalmanFilter>(L, processNoise, measurementNoise, dimensions);
    return 1;
}

int SignalFilterBinding::updateLowPass(lua_State* L) {
    haylen::math::LowPassFilter& filter = lua::Userdata::check<haylen::math::LowPassFilter>(L, 1);
    std::array<float, haylen::math::SignalFilterHelper::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 3, filter.getDimensions(), used);
    filter.update(lua::Stack::read<float>(L, 2), used);
    return pushValues(L, used, vector);
}

int SignalFilterBinding::updateMovingAverage(lua_State* L) {
    haylen::math::MovingAverage& filter = lua::Userdata::check<haylen::math::MovingAverage>(L, 1);
    std::array<float, haylen::math::SignalFilterHelper::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterBinding::updateMedian(lua_State* L) {
    haylen::math::MedianFilter& filter = lua::Userdata::check<haylen::math::MedianFilter>(L, 1);
    std::array<float, haylen::math::SignalFilterHelper::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterBinding::updateHampel(lua_State* L) {
    haylen::math::HampelFilter& filter = lua::Userdata::check<haylen::math::HampelFilter>(L, 1);
    std::array<float, haylen::math::SignalFilterHelper::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

int SignalFilterBinding::updateKalman(lua_State* L) {
    haylen::math::KalmanFilter& filter = lua::Userdata::check<haylen::math::KalmanFilter>(L, 1);
    std::array<float, haylen::math::SignalFilterHelper::kMaxDimensions> values{};
    const std::span<float> used = std::span(values).first(static_cast<std::size_t>(filter.getDimensions()));
    const bool vector = readValues(L, 2, filter.getDimensions(), used);
    filter.update(used);
    return pushValues(L, used, vector);
}

void SignalFilterBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::math::LowPassFilter>(L).function("update", &lua::Binding::native<&updateLowPass>).method<&haylen::math::LowPassFilter::reset>("reset").accessor<&haylen::math::LowPassFilter::getCutoff, &haylen::math::LowPassFilter::setCutoff>("cutoff").property("dimensions", &lua::Binding::method<&haylen::math::LowPassFilter::getDimensions>).install();
    lua::ClassBuilder<haylen::math::MovingAverage>(L).function("update", &lua::Binding::native<&updateMovingAverage>).method<&haylen::math::MovingAverage::reset>("reset").property("window", &lua::Binding::method<&haylen::math::MovingAverage::getWindow>).property("dimensions", &lua::Binding::method<&haylen::math::MovingAverage::getDimensions>).install();
    lua::ClassBuilder<haylen::math::MedianFilter>(L).function("update", &lua::Binding::native<&updateMedian>).method<&haylen::math::MedianFilter::reset>("reset").property("window", &lua::Binding::method<&haylen::math::MedianFilter::getWindow>).property("dimensions", &lua::Binding::method<&haylen::math::MedianFilter::getDimensions>).install();
    lua::ClassBuilder<haylen::math::HampelFilter>(L).function("update", &lua::Binding::native<&updateHampel>).method<&haylen::math::HampelFilter::reset>("reset").property("outlier", &lua::Binding::method<&haylen::math::HampelFilter::isOutlier>).property("window", &lua::Binding::method<&haylen::math::HampelFilter::getWindow>).property("threshold", &lua::Binding::method<&haylen::math::HampelFilter::getThreshold>).property("dimensions", &lua::Binding::method<&haylen::math::HampelFilter::getDimensions>).install();
    lua::ClassBuilder<haylen::math::KalmanFilter>(L).function("update", &lua::Binding::native<&updateKalman>).method<&haylen::math::KalmanFilter::reset>("reset").property("uncertainty", &lua::Binding::method<&haylen::math::KalmanFilter::getUncertainty>).property("dimensions", &lua::Binding::method<&haylen::math::KalmanFilter::getDimensions>).install();
}

void SignalFilterBinding::addFunctions(lua_State* L) {
    for (const auto& [name, function] : {std::pair{"lowPassFilter", &lua::Binding::native<&newLowPass>}, std::pair{"movingAverage", &lua::Binding::native<&newMovingAverage>}, std::pair{"medianFilter", &lua::Binding::native<&newMedian>}, std::pair{"hampelFilter", &lua::Binding::native<&newHampel>}, std::pair{"kalmanFilter", &lua::Binding::native<&newKalman>}}) {
        lua_pushcfunction(L, function);
        lua_setfield(L, -2, name);
    }
}

} // namespace haylen::lua::math
