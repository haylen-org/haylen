#pragma once

#include <lua.hpp>

#include <array>
#include <span>
#include <string_view>

namespace haylen::math {

// Installs the signal filters of `haylen.math`: `LowPassFilter`, `MovingAverage`, `MedianFilter`, `HampelFilter` and `KalmanFilter`, whose `update` takes as many numbers as the filter has dimensions, or a `Vec2` for a filter of two, and returns them filtered the same way.
class SignalFilterLua final {
  public:
    static void install(lua_State* L);

    // Sets the constructors on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kLowPassFields{"cutoff", "dimensions"};
    static constexpr std::array<std::string_view, 2> kWindowFields{"window", "dimensions"};
    static constexpr std::array<std::string_view, 3> kHampelFields{"window", "threshold", "dimensions"};
    static constexpr std::array<std::string_view, 3> kKalmanFields{"processNoise", "measurementNoise", "dimensions"};

    // Reads the values of an update from the index on into the buffer, and returns whether they came as a `Vec2`.
    static bool readValues(lua_State* L, int index, int dimensions, std::span<float> values);
    static int pushValues(lua_State* L, std::span<const float> values, bool vector);
    static void checkOptions(lua_State* L, std::span<const std::string_view> fields);

    static int newLowPass(lua_State* L);
    static int newMovingAverage(lua_State* L);
    static int newMedian(lua_State* L);
    static int newHampel(lua_State* L);
    static int newKalman(lua_State* L);
    static int updateLowPass(lua_State* L);
    static int updateMovingAverage(lua_State* L);
    static int updateMedian(lua_State* L);
    static int updateHampel(lua_State* L);
    static int updateKalman(lua_State* L);
};

} // namespace haylen::math
