#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/EnumNames.hpp"

namespace haylen::animation3d {

// The keys of one property of one target over time in seconds, such as the rotation of the bone "Hips", the morph weights of a face or the intensity of a light. The target names a node or a bone, and the property is `position`, `rotation`, `scale` or `weights` for the transform and the morph weights of a node, or any other path of a property, such as `light.intensity`. A track never changes once made, and every edit returns a new track.
class Track3D final {
  public:
    // What a key holds: a number, a vector, a unit quaternion, a linear color, the weights of the morph targets of a mesh, or a boolean or a string, which change only by steps.
    enum class Type : std::uint8_t {
        Number,
        Vec2,
        Vec3,
        Vec4,
        Quat,
        Color,
        Weights,
        Boolean,
        String,
    };

    // How values between keys come: the value of the key before, a straight line, which is the shortest arc for quaternions, the cubic Hermite spline of glTF, whose keys hold an in tangent, a value and an out tangent, or a cubic Bezier curve through the handles of each key.
    enum class Interpolation : std::uint8_t {
        Step,
        Linear,
        CubicSpline,
        Bezier,
    };

    // How the values are kept: as floats, or as 16-bit steps of the range of each column of values, which takes half the memory and moves a value by at most half a step.
    enum class Storage : std::uint8_t {
        Float,
        Quantized,
    };

    // The keys of a new track. The values hold the components of each key in order, and a cubic spline holds the in tangent, the value and the out tangent of each key in a row, as glTF does, with tangents in units per second. A Bezier track holds four numbers for each component of each key in `handles`, the time and the value of its in handle and of its out handle as offsets from the key, with the in handle at or before the key and the out handle at or after it, each within the time to the key next to it. A weights track holds as many components as its values give for each key. A string track holds its keys in `strings` and writes the index of its key when it samples.
    struct Description {
        std::string target{};
        std::string property{};
        Type type = Type::Number;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times{};
        std::vector<float> values{};
        std::vector<float> handles{};
        std::vector<std::string> strings{};
    };

    // The most keys that an edit makes, such as a resample at a high rate of a long clip.
    static constexpr std::size_t kMaxKeys = std::size_t{1} << 24U;

    // How far a quaternion of a key may be from a length of one, slightly more than the error that quantizing to signed bytes leaves, as the reader of glTF allows, measured the same way.
    static constexpr double kUnitLengthTolerance = 0.00769;

    // Throws `std::invalid_argument` with a sentence that names the track for keys that break the rules of the description.
    explicit Track3D(Description description);

    [[nodiscard]] const std::string& getTarget() const noexcept {
        return target;
    }
    [[nodiscard]] const std::string& getProperty() const noexcept {
        return property;
    }
    [[nodiscard]] Type getType() const noexcept {
        return type;
    }
    [[nodiscard]] Interpolation getInterpolation() const noexcept {
        return interpolation;
    }
    [[nodiscard]] Storage getStorage() const noexcept {
        return storage;
    }

    // The numbers that a sample writes, one for a string track.
    [[nodiscard]] std::size_t getComponentCount() const noexcept {
        return components;
    }
    [[nodiscard]] std::size_t getKeyCount() const noexcept {
        return times.size();
    }
    [[nodiscard]] std::span<const float> getTimes() const noexcept {
        return times;
    }
    [[nodiscard]] float getStartTime() const noexcept {
        return times.front();
    }
    [[nodiscard]] float getEndTime() const noexcept {
        return times.back();
    }

    // The values of the keys as floats, in the layout of the description.
    [[nodiscard]] std::vector<float> getValues() const;
    [[nodiscard]] std::span<const float> getHandles() const noexcept {
        return handles;
    }
    [[nodiscard]] std::span<const std::string> getStrings() const noexcept {
        return strings;
    }

    // The most that quantizing moved a value, zero for floats.
    [[nodiscard]] float getQuantizationError() const noexcept;

    // The bytes that the keys take.
    [[nodiscard]] std::size_t getMemorySize() const noexcept;

    // Returns the last key at or before the time, or the first key for a time before it. The cursor keeps the key of the last call, so a time near the last one finds its key in a step or two, and any cursor finds the same key. Throws `std::invalid_argument` for a time that is not a number.
    [[nodiscard]] std::size_t findKey(float time, std::size_t& cursor) const;

    // Writes the value at a time into `result`, which holds the components of the track. A time before the first key gives the first value and a time after the last key the last value, and a time on a key gives its value exactly. A string track writes the index of its key and a boolean track 0 or 1. Allocates nothing. Throws `std::invalid_argument` for a time that is not a number or a result of another size, and `std::domain_error` for a cubic spline of quaternions that passes through a quaternion of length zero.
    void sample(float time, std::span<float> result, std::size_t& cursor) const;

    // Returns a track whose keys come `seconds` later. Throws `std::invalid_argument` when a key would come before zero.
    [[nodiscard]] Track3D shifted(float seconds) const;

    // Returns the part from `start` to `end`, moved to start at zero, with keys of the exact values at both ends. Throws `std::invalid_argument` for ends that are not finite or not in order.
    [[nodiscard]] Track3D trimmed(float start, float end) const;

    // Returns the keys of this track followed by the keys of `next`, which come `offset` seconds later. Throws `std::invalid_argument` for a track of another target, property, type, interpolation or count of components.
    [[nodiscard]] Track3D appended(const Track3D& next, float offset) const;

    // Returns keys at a fixed rate from `start` to `end`, the last at `end` exactly, with linear interpolation, or steps for a track of steps. Throws `std::invalid_argument` for a rate that is not positive or for more keys than `kMaxKeys`.
    [[nodiscard]] Track3D resampled(float framesPerSecond, float start, float end) const;

    // Returns the fewest keys of a step or linear track that bring back every key within `tolerance`, in the units of the track and in radians for quaternions. A cubic spline or Bezier track keeps its keys unless every key holds the same value with flat tangents or handles, which leaves one key.
    [[nodiscard]] Track3D optimized(float tolerance) const;

    // Returns the track with its values in 16-bit steps. Throws `std::invalid_argument` for a boolean, string or Bezier track, whose values are not a range of numbers.
    [[nodiscard]] Track3D quantized() const;

    // Returns the same keys with `subtract` applied to each value and tangent, which `Clip3D::toAdditive` uses. The value of the key of the reference is subtracted for numbers, vectors, colors and weights, divides the scale, and turns quaternions by its inverse from the left.
    [[nodiscard]] Track3D relativeTo(std::span<const float> reference) const;

    // Describes the track in messages, such as `the "rotation" track of "Hips"`.
    [[nodiscard]] std::string describe() const;

    [[nodiscard]] static std::optional<Type> typeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view typeName(Type value) noexcept;
    [[nodiscard]] static std::optional<Interpolation> interpolationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view interpolationName(Interpolation value) noexcept;
    [[nodiscard]] static std::optional<Storage> storageFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view storageName(Storage value) noexcept;

  private:
    // The range of one column of quantized values: the value of step zero and the size of one step.
    struct Range {
        float offset = 0.0F;
        float step = 0.0F;
    };

    // The parts of a key: the value, and around it the in and out tangents of a cubic spline.
    enum class Slot : std::uint8_t {
        In,
        Value,
        Out,
    };

    static const std::array<std::pair<std::string_view, Type>, 9> kTypeNames;
    static const std::array<std::pair<std::string_view, Interpolation>, 4> kInterpolationNames;
    static const std::array<std::pair<std::string_view, Storage>, 2> kStorageNames;

    [[nodiscard]] std::size_t getSlotCount() const noexcept {
        return interpolation == Interpolation::CubicSpline ? 3 : 1;
    }
    [[nodiscard]] std::size_t getColumnCount() const noexcept {
        return getSlotCount() * components;
    }
    [[nodiscard]] std::size_t getValueIndex(std::size_t key, Slot slot, std::size_t component) const noexcept;
    [[nodiscard]] float readValue(std::size_t index) const noexcept;
    [[nodiscard]] float readSlot(std::size_t key, Slot slot, std::size_t component) const noexcept {
        return readValue(getValueIndex(key, slot, component));
    }

    void validate();
    void validateTimes() const;
    void validateValues();
    void validateHandles() const;
    [[noreturn]] void fail(const std::string& problem) const;

    void sampleInterval(std::size_t key, float time, std::span<float> result) const;
    void sampleCubic(std::size_t key, float time, std::span<float> result) const;
    void sampleBezier(std::size_t key, float time, std::span<float> result) const;
    void writeKey(std::size_t key, std::span<float> result) const noexcept;

    // The derivative of a cubic spline in units per second at a time, which is zero outside the keys.
    void cubicDerivative(float time, std::span<float> result) const;

    // The parameter of a Bezier segment whose curve of time reaches `share` of the segment, with the shares of time of the two inner handles.
    [[nodiscard]] static float solveBezier(float share, float outShare, float inShare) noexcept;

    [[nodiscard]] Track3D withKeys(std::vector<float> keyTimes, std::vector<float> keyValues, std::vector<float> keyHandles, std::vector<std::string> keyStrings, Interpolation keyInterpolation) const;
    [[nodiscard]] Track3D withKeyAt(float time) const;
    [[nodiscard]] Track3D optimizedSteps(float tolerance) const;
    [[nodiscard]] Track3D optimizedLines(float tolerance) const;
    [[nodiscard]] float measureError(std::size_t from, std::size_t to, std::size_t key) const;
    [[nodiscard]] bool isConstant(float tolerance) const;

    std::string target;
    std::string property;
    Type type = Type::Number;
    Interpolation interpolation = Interpolation::Linear;
    Storage storage = Storage::Float;
    std::size_t components = 1;
    std::vector<float> times;
    std::vector<float> values;
    std::vector<std::uint16_t> steps;
    std::vector<Range> ranges;
    std::vector<float> handles;
    std::vector<std::string> strings;
};

} // namespace haylen::animation3d

namespace haylen::core {

template <> struct EnumNames<animation3d::Track3D::Type> {
    static std::optional<animation3d::Track3D::Type> fromName(std::string_view name) {
        return animation3d::Track3D::typeFromName(name);
    }
    static std::string_view name(animation3d::Track3D::Type value) {
        return animation3d::Track3D::typeName(value);
    }
};

template <> struct EnumNames<animation3d::Track3D::Interpolation> {
    static std::optional<animation3d::Track3D::Interpolation> fromName(std::string_view name) {
        return animation3d::Track3D::interpolationFromName(name);
    }
    static std::string_view name(animation3d::Track3D::Interpolation value) {
        return animation3d::Track3D::interpolationName(value);
    }
};

template <> struct EnumNames<animation3d::Track3D::Storage> {
    static std::optional<animation3d::Track3D::Storage> fromName(std::string_view name) {
        return animation3d::Track3D::storageFromName(name);
    }
    static std::string_view name(animation3d::Track3D::Storage value) {
        return animation3d::Track3D::storageName(value);
    }
};

} // namespace haylen::core
