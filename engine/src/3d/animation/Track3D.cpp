#include "haylen/3d/animation/Track3D.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <stdexcept>
#include <utility>

#include "haylen/math/Math.hpp"
#include "haylen/math/Quat.hpp"

namespace haylen::animation3d {

const std::array<std::pair<std::string_view, Track3D::Type>, 9> Track3D::kTypeNames{{{"number", Type::Number}, {"vec2", Type::Vec2}, {"vec3", Type::Vec3}, {"vec4", Type::Vec4}, {"quat", Type::Quat}, {"color", Type::Color}, {"weights", Type::Weights}, {"boolean", Type::Boolean}, {"string", Type::String}}};
const std::array<std::pair<std::string_view, Track3D::Interpolation>, 4> Track3D::kInterpolationNames{{{"step", Interpolation::Step}, {"linear", Interpolation::Linear}, {"cubicSpline", Interpolation::CubicSpline}, {"bezier", Interpolation::Bezier}}};
const std::array<std::pair<std::string_view, Track3D::Storage>, 2> Track3D::kStorageNames{{{"float", Storage::Float}, {"quantized", Storage::Quantized}}};

std::optional<Track3D::Type> Track3D::typeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kTypeNames, name, &std::pair<std::string_view, Type>::first);
    return found != kTypeNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Track3D::typeName(Type value) noexcept {
    return std::ranges::find(kTypeNames, value, &std::pair<std::string_view, Type>::second)->first;
}

std::optional<Track3D::Interpolation> Track3D::interpolationFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kInterpolationNames, name, &std::pair<std::string_view, Interpolation>::first);
    return found != kInterpolationNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Track3D::interpolationName(Interpolation value) noexcept {
    return std::ranges::find(kInterpolationNames, value, &std::pair<std::string_view, Interpolation>::second)->first;
}

std::optional<Track3D::Storage> Track3D::storageFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kStorageNames, name, &std::pair<std::string_view, Storage>::first);
    return found != kStorageNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Track3D::storageName(Storage value) noexcept {
    return std::ranges::find(kStorageNames, value, &std::pair<std::string_view, Storage>::second)->first;
}

Track3D::Track3D(Description description) : target(std::move(description.target)), property(std::move(description.property)), type(description.type), interpolation(description.interpolation), times(std::move(description.times)), values(std::move(description.values)), handles(std::move(description.handles)), strings(std::move(description.strings)) {
    validate();
}

std::string Track3D::describe() const {
    if (target.empty()) {
        return std::format("the \"{}\" track", property);
    }

    return std::format("the \"{}\" track of \"{}\"", property, target);
}

void Track3D::fail(const std::string& problem) const {
    std::string message = describe() + " " + problem + ".";
    message.front() = 'T';
    throw std::invalid_argument(message);
}

void Track3D::validate() {
    if (property.empty()) {
        throw std::invalid_argument("A track needs the property it animates, such as \"position\".");
    }

    if (times.empty()) {
        fail("has no keys, while a track needs at least one key");
    }

    validateTimes();
    const bool onlySteps = type == Type::Boolean || type == Type::String;
    if (onlySteps && interpolation != Interpolation::Step) {
        fail(std::format("holds {} values, which change only by steps, so its interpolation is \"step\"", typeName(type)));
    }

    if (type == Type::Quat && interpolation == Interpolation::Bezier) {
        fail("holds quaternions, which a Bezier curve cannot shape, so its interpolation is \"step\", \"linear\" or \"cubicSpline\"");
    }

    if (type == Type::String) {
        if (strings.size() != times.size()) {
            fail(std::format("has {} keys and {} strings, while each key of a string track holds one string", times.size(), strings.size()));
        }

        if (!values.empty() || !handles.empty()) {
            fail("holds numbers, while a string track holds only its strings");
        }

        return;
    }

    if (!strings.empty()) {
        fail("holds strings, while only a string track takes them");
    }

    validateValues();
    validateHandles();
}

void Track3D::validateTimes() const {
    for (std::size_t key = 0; key < times.size(); ++key) {
        if (!std::isfinite(times[key]) || times[key] < 0.0F) {
            fail(std::format("has the time {} at key {}, while the times of a track are finite and zero or more seconds", times[key], key));
        }

        if (key > 0 && times[key] < times[key - 1]) {
            fail(std::format("has key {} at {} seconds, before the key before it, while the times of a track never go back", key, times[key]));
        }
    }
}

void Track3D::validateValues() {
    const std::size_t keys = times.size();
    const std::size_t slots = getSlotCount();
    switch (type) {
    case Type::Number:
    case Type::Boolean:
        components = 1;
        break;
    case Type::Vec2:
        components = 2;
        break;
    case Type::Vec3:
        components = 3;
        break;
    case Type::Vec4:
    case Type::Quat:
    case Type::Color:
        components = 4;
        break;
    case Type::Weights:
        if (values.empty() || values.size() % (keys * slots) != 0) {
            fail(std::format("holds {} values, which do not give the same number of weights to each of its {} keys", values.size(), keys));
        }

        components = values.size() / (keys * slots);
        break;
    case Type::String:
        break;
    }

    const std::size_t needed = keys * slots * components;
    if (values.size() != needed) {
        fail(std::format("holds {} values, while its {} keys of {} components need {}", values.size(), keys, components, needed));
    }

    for (std::size_t index = 0; index < values.size(); ++index) {
        if (!std::isfinite(values[index])) {
            fail(std::format("holds the value {} at place {} of its values, while values are finite numbers", values[index], index));
        }
    }

    if (type == Type::Boolean) {
        for (std::size_t key = 0; key < keys; ++key) {
            if (values[key] != 0.0F && values[key] != 1.0F) {
                fail(std::format("holds the value {} at key {}, while a boolean track holds 0 or 1", values[key], key));
            }
        }
    }

    // Rotations keep a length of one, within the tolerance of glTF, and become exactly unit once checked.
    if (type == Type::Quat) {
        for (std::size_t key = 0; key < keys; ++key) {
            float* quaternion = values.data() + getValueIndex(key, Slot::Value, 0);
            double squared = 0.0;
            for (std::size_t component = 0; component < 4; ++component) {
                squared += static_cast<double>(quaternion[component]) * quaternion[component];
            }

            const double length = std::sqrt(squared);
            if (std::abs(length - 1.0) > kUnitLengthTolerance) {
                fail(std::format("holds a quaternion of length {} at key {}, while rotations are unit quaternions", static_cast<float>(length), key));
            }

            for (std::size_t component = 0; component < 4; ++component) {
                quaternion[component] = static_cast<float>(quaternion[component] / length);
            }
        }
    }
}

void Track3D::validateHandles() const {
    if (interpolation != Interpolation::Bezier) {
        if (!handles.empty()) {
            fail("holds Bezier handles, while only a Bezier track takes them");
        }

        return;
    }

    const std::size_t keys = times.size();
    const std::size_t needed = keys * components * 4;
    if (handles.size() != needed) {
        fail(std::format("holds {} numbers of handles, while its {} keys of {} components need {}", handles.size(), keys, components, needed));
    }

    for (std::size_t key = 0; key < keys; ++key) {
        for (std::size_t component = 0; component < components; ++component) {
            const float* handle = handles.data() + (key * components + component) * 4;
            if (!std::isfinite(handle[0]) || !std::isfinite(handle[1]) || !std::isfinite(handle[2]) || !std::isfinite(handle[3])) {
                fail(std::format("has a handle of key {} that is not a finite number", key));
            }

            if (handle[0] > 0.0F || handle[2] < 0.0F) {
                fail(std::format("has a handle of key {} on the wrong side of it, while the in handle comes at or before its key and the out handle at or after it", key));
            }

            // A handle that reaches past the key next to it would turn the curve back in time.
            const bool outPasses = key + 1 < keys && handle[2] > times[key + 1] - times[key];
            const bool inPasses = key > 0 && -handle[0] > times[key] - times[key - 1];
            if (outPasses || inPasses) {
                fail(std::format("has a handle of key {} that reaches past the key next to it, so the curve would go back in time", key));
            }
        }
    }
}

std::size_t Track3D::getValueIndex(std::size_t key, Slot slot, std::size_t component) const noexcept {
    const std::size_t slotIndex = interpolation == Interpolation::CubicSpline ? static_cast<std::size_t>(slot) : 0;
    return (key * getSlotCount() + slotIndex) * components + component;
}

float Track3D::readValue(std::size_t index) const noexcept {
    if (storage == Storage::Float) {
        return values[index];
    }

    const Range& range = ranges[index % getColumnCount()];
    return range.offset + static_cast<float>(steps[index]) * range.step;
}

std::vector<float> Track3D::getValues() const {
    const std::size_t count = type == Type::String ? 0 : times.size() * getColumnCount();
    std::vector<float> result(count);
    for (std::size_t index = 0; index < count; ++index) {
        result[index] = readValue(index);
    }

    return result;
}

float Track3D::getQuantizationError() const noexcept {
    float error = 0.0F;
    for (const Range& range : ranges) {
        error = std::max(error, range.step * 0.5F);
    }

    return error;
}

std::size_t Track3D::getMemorySize() const noexcept {
    std::size_t size = times.size() * sizeof(float) + values.size() * sizeof(float) + steps.size() * sizeof(std::uint16_t) + ranges.size() * sizeof(Range) + handles.size() * sizeof(float);
    for (const std::string& text : strings) {
        size += text.size();
    }

    return size;
}

std::size_t Track3D::findKey(float time, std::size_t& cursor) const {
    if (std::isnan(time)) {
        throw std::invalid_argument("The time of a sample is not a number.");
    }

    const std::size_t count = times.size();
    if (time < times.front()) {
        cursor = 0;
        return 0;
    }

    // The key of the last sample, or a key next to it, usually holds the time, and a search finds any other.
    std::size_t key = std::min(cursor, count - 1);
    if (times[key] <= time) {
        for (int step = 0; step < 2 && key + 1 < count && times[key + 1] <= time; ++step) {
            ++key;
        }

        if (key + 1 < count && times[key + 1] <= time) {
            key = static_cast<std::size_t>(std::upper_bound(times.begin() + static_cast<std::ptrdiff_t>(key) + 1, times.end(), time) - times.begin()) - 1;
        }
    } else if (key > 0 && times[key - 1] <= time) {
        --key;
    } else {
        key = static_cast<std::size_t>(std::upper_bound(times.begin(), times.begin() + static_cast<std::ptrdiff_t>(key), time) - times.begin()) - 1;
    }

    cursor = key;
    return key;
}

void Track3D::sample(float time, std::span<float> result, std::size_t& cursor) const {
    if (result.size() != components) {
        std::string message = std::format("a sample of {} takes {} numbers, while {} were given.", describe(), components, result.size());
        message.front() = 'A';
        throw std::invalid_argument(message);
    }

    const std::size_t key = findKey(time, cursor);
    if (key + 1 == times.size() || time <= times[key] || interpolation == Interpolation::Step || type == Type::String) {
        writeKey(key, result);
        return;
    }

    sampleInterval(key, time, result);
}

void Track3D::writeKey(std::size_t key, std::span<float> result) const noexcept {
    if (type == Type::String) {
        result[0] = static_cast<float>(key);
        return;
    }

    for (std::size_t component = 0; component < components; ++component) {
        result[component] = readSlot(key, Slot::Value, component);
    }

    // Steps of a quantized rotation leave it a little off a length of one.
    if (type == Type::Quat && storage == Storage::Quantized) {
        const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] + result[2] * result[2] + result[3] * result[3]);
        for (float& component : result) {
            component /= length;
        }
    }
}

void Track3D::sampleInterval(std::size_t key, float time, std::span<float> result) const {
    if (interpolation == Interpolation::CubicSpline) {
        sampleCubic(key, time, result);
        return;
    }

    if (interpolation == Interpolation::Bezier) {
        sampleBezier(key, time, result);
        return;
    }

    const float share = (time - times[key]) / (times[key + 1] - times[key]);
    if (type == Type::Quat) {
        const math::Quat from = math::Quat{readSlot(key, Slot::Value, 0), readSlot(key, Slot::Value, 1), readSlot(key, Slot::Value, 2), readSlot(key, Slot::Value, 3)}.getNormalized();
        const math::Quat to = math::Quat{readSlot(key + 1, Slot::Value, 0), readSlot(key + 1, Slot::Value, 1), readSlot(key + 1, Slot::Value, 2), readSlot(key + 1, Slot::Value, 3)}.getNormalized();
        const math::Quat between = math::Quat::slerp(from, to, share);
        result[0] = between.x;
        result[1] = between.y;
        result[2] = between.z;
        result[3] = between.w;
        return;
    }

    for (std::size_t component = 0; component < components; ++component) {
        result[component] = math::Math::lerp(readSlot(key, Slot::Value, component), readSlot(key + 1, Slot::Value, component), share);
    }
}

// The cubic Hermite spline of glTF: the values of both keys with the out tangent of the first and the in tangent of the second, scaled by the time between them.
void Track3D::sampleCubic(std::size_t key, float time, std::span<float> result) const {
    const float duration = times[key + 1] - times[key];
    const float share = (time - times[key]) / duration;
    const float square = share * share;
    const float cube = square * share;
    const float fromWeight = 2.0F * cube - 3.0F * square + 1.0F;
    const float outWeight = duration * (cube - 2.0F * square + share);
    const float toWeight = -2.0F * cube + 3.0F * square;
    const float inWeight = duration * (cube - square);
    for (std::size_t component = 0; component < components; ++component) {
        result[component] = fromWeight * readSlot(key, Slot::Value, component) + outWeight * readSlot(key, Slot::Out, component) + toWeight * readSlot(key + 1, Slot::Value, component) + inWeight * readSlot(key + 1, Slot::In, component);
    }

    if (type != Type::Quat) {
        return;
    }

    const float length = std::sqrt(result[0] * result[0] + result[1] * result[1] + result[2] * result[2] + result[3] * result[3]);
    if (length == 0.0F) {
        std::string message = std::format("{} passes through a quaternion of length zero at {} seconds, so it holds no rotation there. Make the tangents of its keys smaller.", describe(), time);
        message.front() = 'T';
        throw std::domain_error(message);
    }

    for (float& component : result) {
        component /= length;
    }
}

void Track3D::sampleBezier(std::size_t key, float time, std::span<float> result) const {
    const float duration = times[key + 1] - times[key];
    const float share = (time - times[key]) / duration;
    for (std::size_t component = 0; component < components; ++component) {
        const float* from = handles.data() + (key * components + component) * 4;
        const float* to = handles.data() + ((key + 1) * components + component) * 4;
        const float parameter = solveBezier(share, from[2] / duration, 1.0F + to[0] / duration);
        const float rest = 1.0F - parameter;
        const float start = readSlot(key, Slot::Value, component);
        const float end = readSlot(key + 1, Slot::Value, component);
        result[component] = rest * rest * rest * start + 3.0F * rest * rest * parameter * (start + from[3]) + 3.0F * rest * parameter * parameter * (end + to[1]) + parameter * parameter * parameter * end;
    }
}

// The curve of time runs from 0 to 1 with its inner points at `outShare` and `inShare`, both within the segment, so it never goes back and one parameter reaches each share. Newton steps find it, with bisection inside the bracket whenever a step would leave it.
float Track3D::solveBezier(float share, float outShare, float inShare) noexcept {
    constexpr int kIterations = 24;
    constexpr float kPrecision = 1e-7F;
    float low = 0.0F;
    float high = 1.0F;
    float parameter = share;
    for (int iteration = 0; iteration < kIterations; ++iteration) {
        const float rest = 1.0F - parameter;
        const float reached = 3.0F * rest * rest * parameter * outShare + 3.0F * rest * parameter * parameter * inShare + parameter * parameter * parameter;
        const float error = reached - share;
        if (std::abs(error) <= kPrecision) {
            return parameter;
        }

        if (error > 0.0F) {
            high = parameter;
        } else {
            low = parameter;
        }

        const float slope = 3.0F * rest * rest * outShare + 6.0F * rest * parameter * (inShare - outShare) + 3.0F * parameter * parameter * (1.0F - inShare);
        float next = slope > kPrecision ? parameter - error / slope : (low + high) * 0.5F;
        if (next <= low || next >= high) {
            next = (low + high) * 0.5F;
        }

        parameter = next;
    }

    return parameter;
}

void Track3D::cubicDerivative(float time, std::span<float> result) const {
    std::ranges::fill(result, 0.0F);
    std::size_t cursor = 0;
    const std::size_t key = findKey(time, cursor);
    if (time < times.front() || key + 1 == times.size()) {
        return;
    }

    const float duration = times[key + 1] - times[key];
    const float share = (time - times[key]) / duration;
    const float square = share * share;
    for (std::size_t component = 0; component < components; ++component) {
        const float change = (6.0F * square - 6.0F * share) * readSlot(key, Slot::Value, component) + duration * (3.0F * square - 4.0F * share + 1.0F) * readSlot(key, Slot::Out, component) + (6.0F * share - 6.0F * square) * readSlot(key + 1, Slot::Value, component) + duration * (3.0F * square - 2.0F * share) * readSlot(key + 1, Slot::In, component);
        result[component] = change / duration;
    }
}

Track3D Track3D::withKeys(std::vector<float> keyTimes, std::vector<float> keyValues, std::vector<float> keyHandles, std::vector<std::string> keyStrings, Interpolation keyInterpolation) const {
    return Track3D({.target = target, .property = property, .type = type, .interpolation = keyInterpolation, .times = std::move(keyTimes), .values = std::move(keyValues), .handles = std::move(keyHandles), .strings = std::move(keyStrings)});
}

Track3D Track3D::shifted(float seconds) const {
    std::vector<float> keyTimes = times;
    for (float& time : keyTimes) {
        time += seconds;
    }

    return withKeys(std::move(keyTimes), getValues(), handles, strings, interpolation);
}

// Splits the segment that holds the time at a new key that keeps the curve: the value there for steps and lines, the value with its tangent for a cubic spline, and the two halves of the Bezier segment, whose handles change on both sides. A time on a key or outside the keys keeps the curve with a key of the value there.
Track3D Track3D::withKeyAt(float time) const {
    std::size_t cursor = 0;
    const std::size_t key = findKey(time, cursor);
    if (time == times[key]) {
        return withKeys(times, getValues(), handles, strings, interpolation);
    }

    const std::size_t place = time < times.front() ? 0 : key + 1;
    const bool inside = time > times.front() && key + 1 < times.size();
    std::vector<float> keyTimes = times;
    keyTimes.insert(keyTimes.begin() + static_cast<std::ptrdiff_t>(place), time);
    std::vector<std::string> keyStrings = strings;
    if (type == Type::String) {
        keyStrings.insert(keyStrings.begin() + static_cast<std::ptrdiff_t>(place), strings[key]);
        return withKeys(std::move(keyTimes), {}, {}, std::move(keyStrings), interpolation);
    }

    std::vector<float> value(components);
    sample(time, value, cursor);
    std::vector<float> inserted;
    if (interpolation == Interpolation::CubicSpline) {
        std::vector<float> slope(components);
        cubicDerivative(time, slope);
        inserted.insert(inserted.end(), slope.begin(), slope.end());
        inserted.insert(inserted.end(), value.begin(), value.end());
        inserted.insert(inserted.end(), slope.begin(), slope.end());
    } else {
        inserted = value;
    }

    std::vector<float> keyValues = getValues();
    keyValues.insert(keyValues.begin() + static_cast<std::ptrdiff_t>(place * getColumnCount()), inserted.begin(), inserted.end());
    std::vector<float> keyHandles = handles;
    if (interpolation == Interpolation::Bezier) {
        keyHandles.insert(keyHandles.begin() + static_cast<std::ptrdiff_t>(place * components * 4), components * 4, 0.0F);
    }

    // A key outside the others holds the value of the end it lies past, so the tangent or handle of that end toward it lies flat, as the curve outside the keys does.
    if (!inside) {
        const std::size_t neighbor = place == 0 ? 1 : place - 1;
        const Slot facing = place == 0 ? Slot::In : Slot::Out;
        for (std::size_t component = 0; component < components; ++component) {
            if (interpolation == Interpolation::CubicSpline) {
                keyValues[(neighbor * 3 + static_cast<std::size_t>(facing)) * components + component] = 0.0F;
            }

            if (interpolation == Interpolation::Bezier) {
                float* handle = keyHandles.data() + (neighbor * components + component) * 4 + (place == 0 ? 0 : 2);
                handle[0] = 0.0F;
                handle[1] = 0.0F;
            }
        }
    }

    if (interpolation != Interpolation::Bezier || !inside) {
        return withKeys(std::move(keyTimes), std::move(keyValues), std::move(keyHandles), {}, interpolation);
    }

    // The halves of each Bezier segment come from de Casteljau's construction at the parameter where its curve of time reaches the new key.
    const float duration = times[key + 1] - times[key];
    for (std::size_t component = 0; component < components; ++component) {
        float* from = keyHandles.data() + (key * components + component) * 4;
        float* middle = keyHandles.data() + ((key + 1) * components + component) * 4;
        float* to = keyHandles.data() + ((key + 2) * components + component) * 4;
        const float startTime = times[key];
        const float startValue = readSlot(key, Slot::Value, component);
        const float endTime = times[key + 1];
        const float endValue = readSlot(key + 1, Slot::Value, component);
        const std::array<float, 8> points{startTime, startValue, startTime + from[2], startValue + from[3], endTime + to[0], endValue + to[1], endTime, endValue};
        const float parameter = solveBezier((time - startTime) / duration, from[2] / duration, 1.0F + to[0] / duration);
        std::array<float, 2> first{};
        std::array<float, 2> second{};
        std::array<float, 2> third{};
        std::array<float, 2> left{};
        std::array<float, 2> right{};
        std::array<float, 2> split{};
        for (std::size_t axis = 0; axis < 2; ++axis) {
            first[axis] = math::Math::lerp(points[axis], points[2 + axis], parameter);
            second[axis] = math::Math::lerp(points[2 + axis], points[4 + axis], parameter);
            third[axis] = math::Math::lerp(points[4 + axis], points[6 + axis], parameter);
            left[axis] = math::Math::lerp(first[axis], second[axis], parameter);
            right[axis] = math::Math::lerp(second[axis], third[axis], parameter);
            split[axis] = math::Math::lerp(left[axis], right[axis], parameter);
        }

        // The time of the split is the time of the key, and its value the value that the sample gave. Rounding may put a handle a hair past its segment, where the construction never puts it.
        split[0] = time;
        split[1] = value[component];
        from[2] = std::clamp(first[0] - startTime, 0.0F, time - startTime);
        from[3] = first[1] - startValue;
        middle[0] = std::clamp(left[0] - split[0], startTime - time, 0.0F);
        middle[1] = left[1] - split[1];
        middle[2] = std::clamp(right[0] - split[0], 0.0F, endTime - time);
        middle[3] = right[1] - split[1];
        to[0] = std::clamp(third[0] - endTime, time - endTime, 0.0F);
        to[1] = third[1] - endValue;
    }

    return withKeys(std::move(keyTimes), std::move(keyValues), std::move(keyHandles), {}, interpolation);
}

Track3D Track3D::trimmed(float start, float end) const {
    if (!std::isfinite(start) || !std::isfinite(end) || start < 0.0F || end < start) {
        throw std::invalid_argument(std::format("A trim runs from a start of zero or more seconds to an end at or after it, while it was given {} and {}.", start, end));
    }

    const Track3D split = withKeyAt(start).withKeyAt(end);
    std::size_t cursor = 0;
    const std::size_t first = split.findKey(start, cursor);
    const std::size_t last = split.findKey(end, cursor);
    const std::size_t columns = split.type == Type::String ? 0 : split.getColumnCount();
    std::vector<float> keyTimes(split.times.begin() + static_cast<std::ptrdiff_t>(first), split.times.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    for (float& time : keyTimes) {
        time -= start;
    }

    std::vector<float> keyValues(split.values.begin() + static_cast<std::ptrdiff_t>(first * columns), split.values.begin() + static_cast<std::ptrdiff_t>((last + 1) * columns));
    std::vector<float> keyHandles;
    if (interpolation == Interpolation::Bezier) {
        keyHandles.assign(split.handles.begin() + static_cast<std::ptrdiff_t>(first * components * 4), split.handles.begin() + static_cast<std::ptrdiff_t>((last + 1) * components * 4));
    }

    std::vector<std::string> keyStrings;
    if (type == Type::String) {
        keyStrings.assign(split.strings.begin() + static_cast<std::ptrdiff_t>(first), split.strings.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    }

    return withKeys(std::move(keyTimes), std::move(keyValues), std::move(keyHandles), std::move(keyStrings), interpolation);
}

Track3D Track3D::appended(const Track3D& next, float offset) const {
    if (next.target != target || next.property != property || next.type != type || next.interpolation != interpolation || next.components != components) {
        std::string message = std::format("{} joins only a track of the same target, property, type, interpolation and number of components, while {} differs.", describe(), next.describe());
        message.front() = 'T';
        throw std::invalid_argument(message);
    }

    std::vector<float> keyTimes = times;
    for (const float time : next.times) {
        keyTimes.push_back(time + offset);
    }

    std::vector<float> keyValues = getValues();
    const std::vector<float> nextValues = next.getValues();
    keyValues.insert(keyValues.end(), nextValues.begin(), nextValues.end());
    std::vector<float> keyHandles = handles;
    keyHandles.insert(keyHandles.end(), next.handles.begin(), next.handles.end());
    std::vector<std::string> keyStrings = strings;
    keyStrings.insert(keyStrings.end(), next.strings.begin(), next.strings.end());
    return withKeys(std::move(keyTimes), std::move(keyValues), std::move(keyHandles), std::move(keyStrings), interpolation);
}

Track3D Track3D::resampled(float framesPerSecond, float start, float end) const {
    if (!std::isfinite(framesPerSecond) || framesPerSecond <= 0.0F) {
        throw std::invalid_argument(std::format("A resample takes a positive number of frames per second, while it was given {}.", framesPerSecond));
    }

    if (!std::isfinite(start) || !std::isfinite(end) || start < 0.0F || end < start) {
        throw std::invalid_argument(std::format("A resample runs from a start of zero or more seconds to an end at or after it, while it was given {} and {}.", start, end));
    }

    const double frames = std::floor((static_cast<double>(end) - start) * framesPerSecond);
    if (frames + 2.0 > static_cast<double>(kMaxKeys)) {
        std::string message = std::format("a resample of {} at {} frames per second would make more than {} keys.", describe(), framesPerSecond, kMaxKeys);
        message.front() = 'A';
        throw std::invalid_argument(message);
    }

    std::vector<float> keyTimes;
    keyTimes.reserve(static_cast<std::size_t>(frames) + 2);
    for (std::size_t frame = 0; frame <= static_cast<std::size_t>(frames); ++frame) {
        const float time = static_cast<float>(start + static_cast<double>(frame) / framesPerSecond);
        if (time < end) {
            keyTimes.push_back(time);
        }
    }

    keyTimes.push_back(end);
    std::vector<float> keyValues;
    std::vector<std::string> keyStrings;
    std::vector<float> value(components);
    std::size_t cursor = 0;
    for (const float time : keyTimes) {
        sample(time, value, cursor);
        if (type == Type::String) {
            keyStrings.push_back(strings[static_cast<std::size_t>(value[0])]);
        } else {
            keyValues.insert(keyValues.end(), value.begin(), value.end());
        }
    }

    const Interpolation keyInterpolation = interpolation == Interpolation::Step ? Interpolation::Step : Interpolation::Linear;
    return withKeys(std::move(keyTimes), std::move(keyValues), {}, std::move(keyStrings), keyInterpolation);
}

Track3D Track3D::optimized(float tolerance) const {
    if (!std::isfinite(tolerance) || tolerance < 0.0F) {
        throw std::invalid_argument(std::format("An optimization takes a tolerance of zero or more, while it was given {}.", tolerance));
    }

    switch (interpolation) {
    case Interpolation::Step:
        return optimizedSteps(tolerance);
    case Interpolation::Linear:
        return optimizedLines(tolerance);
    case Interpolation::CubicSpline:
    case Interpolation::Bezier:
        break;
    }

    if (!isConstant(tolerance)) {
        return withKeys(times, getValues(), handles, strings, interpolation);
    }

    std::vector<float> keyValues(getColumnCount(), 0.0F);
    for (std::size_t component = 0; component < components; ++component) {
        keyValues[getValueIndex(0, Slot::Value, component)] = readSlot(0, Slot::Value, component);
    }

    const std::vector<float> keyHandles = interpolation == Interpolation::Bezier ? std::vector<float>(components * 4, 0.0F) : std::vector<float>{};
    return withKeys({times.front()}, std::move(keyValues), keyHandles, {}, interpolation);
}

bool Track3D::isConstant(float tolerance) const {
    for (std::size_t key = 1; key < times.size(); ++key) {
        if (measureError(0, 0, key) > tolerance) {
            return false;
        }
    }

    for (std::size_t key = 0; key < times.size(); ++key) {
        for (std::size_t component = 0; component < components; ++component) {
            if (interpolation == Interpolation::CubicSpline && (std::abs(readSlot(key, Slot::In, component)) > tolerance || std::abs(readSlot(key, Slot::Out, component)) > tolerance)) {
                return false;
            }

            if (interpolation == Interpolation::Bezier) {
                const float* handle = handles.data() + (key * components + component) * 4;
                if (std::abs(handle[1]) > tolerance || std::abs(handle[3]) > tolerance) {
                    return false;
                }
            }
        }
    }

    return true;
}

// Measures how far key `key` lies from the line between keys `from` and `to` at its time, in radians for quaternions and in the largest difference of a component otherwise. A line of no length stands for the value of `to`.
float Track3D::measureError(std::size_t from, std::size_t to, std::size_t key) const {
    if (type == Type::String) {
        return strings[key] == strings[to] ? 0.0F : std::numeric_limits<float>::infinity();
    }

    const float duration = times[to] - times[from];
    const float share = duration > 0.0F ? (times[key] - times[from]) / duration : 1.0F;
    if (type == Type::Quat) {
        const math::Quat start = math::Quat{readSlot(from, Slot::Value, 0), readSlot(from, Slot::Value, 1), readSlot(from, Slot::Value, 2), readSlot(from, Slot::Value, 3)}.getNormalized();
        const math::Quat end = math::Quat{readSlot(to, Slot::Value, 0), readSlot(to, Slot::Value, 1), readSlot(to, Slot::Value, 2), readSlot(to, Slot::Value, 3)}.getNormalized();
        const math::Quat actual = math::Quat{readSlot(key, Slot::Value, 0), readSlot(key, Slot::Value, 1), readSlot(key, Slot::Value, 2), readSlot(key, Slot::Value, 3)}.getNormalized();
        return math::Quat::angleBetween(math::Quat::slerp(start, end, share), actual);
    }

    float error = 0.0F;
    for (std::size_t component = 0; component < components; ++component) {
        const float expected = math::Math::lerp(readSlot(from, Slot::Value, component), readSlot(to, Slot::Value, component), share);
        error = std::max(error, std::abs(expected - readSlot(key, Slot::Value, component)));
    }

    return error;
}

// Keeps a key of steps only when it shows and changes the value: a key that another key at the same time follows never shows, and a key equal to the key kept before it changes nothing.
Track3D Track3D::optimizedSteps(float tolerance) const {
    std::vector<std::size_t> kept;
    for (std::size_t key = 0; key < times.size(); ++key) {
        if (key + 1 < times.size() && times[key + 1] == times[key]) {
            continue;
        }

        const bool same = !kept.empty() && (type == Type::Boolean ? readSlot(key, Slot::Value, 0) == readSlot(kept.back(), Slot::Value, 0) : measureError(kept.back(), kept.back(), key) <= tolerance);
        if (!same) {
            kept.push_back(key);
        }
    }

    std::vector<float> keyTimes;
    std::vector<float> keyValues;
    std::vector<std::string> keyStrings;
    for (const std::size_t key : kept) {
        keyTimes.push_back(times[key]);
        if (type == Type::String) {
            keyStrings.push_back(strings[key]);
            continue;
        }

        for (std::size_t component = 0; component < components; ++component) {
            keyValues.push_back(readSlot(key, Slot::Value, component));
        }
    }

    return withKeys(std::move(keyTimes), std::move(keyValues), {}, std::move(keyStrings), interpolation);
}

// Keeps the fewest keys of a line that bring back every key within the tolerance: each span between two kept keys keeps the key that lies farthest from it while that key lies past the tolerance, which splits the span in two, and a span of two equal keys becomes one key.
Track3D Track3D::optimizedLines(float tolerance) const {
    const std::size_t count = times.size();
    std::vector<bool> kept(count, false);
    kept.front() = true;
    kept.back() = true;
    std::vector<std::pair<std::size_t, std::size_t>> spans{{0, count - 1}};
    while (!spans.empty()) {
        const auto [from, to] = spans.back();
        spans.pop_back();
        float farthest = tolerance;
        std::optional<std::size_t> split;
        for (std::size_t key = from + 1; key < to; ++key) {
            const float error = measureError(from, to, key);
            if (error > farthest) {
                farthest = error;
                split = key;
            }
        }

        if (split) {
            kept[*split] = true;
            spans.emplace_back(from, *split);
            spans.emplace_back(*split, to);
        }
    }

    std::vector<std::size_t> keys;
    for (std::size_t key = 0; key < count; ++key) {
        if (kept[key]) {
            keys.push_back(key);
        }
    }

    if (keys.size() == 2 && measureError(keys.front(), keys.front(), keys.back()) <= tolerance) {
        keys.pop_back();
    }

    std::vector<float> keyTimes;
    std::vector<float> keyValues;
    for (const std::size_t key : keys) {
        keyTimes.push_back(times[key]);
        for (std::size_t component = 0; component < components; ++component) {
            keyValues.push_back(readSlot(key, Slot::Value, component));
        }
    }

    return withKeys(std::move(keyTimes), std::move(keyValues), {}, {}, interpolation);
}

Track3D Track3D::quantized() const {
    if (type == Type::Boolean || type == Type::String || interpolation == Interpolation::Bezier) {
        std::string message = std::format("{} cannot be quantized, since only numbers that interpolate by steps, lines or cubic splines can.", describe());
        message.front() = 'T';
        throw std::invalid_argument(message);
    }

    Track3D result = withKeys(times, getValues(), {}, {}, interpolation);
    const std::size_t columns = result.getColumnCount();
    result.ranges.assign(columns, Range{});
    for (std::size_t column = 0; column < columns; ++column) {
        float low = std::numeric_limits<float>::infinity();
        float high = -std::numeric_limits<float>::infinity();
        for (std::size_t index = column; index < result.values.size(); index += columns) {
            low = std::min(low, result.values[index]);
            high = std::max(high, result.values[index]);
        }

        result.ranges[column] = {.offset = low, .step = (high - low) / 65535.0F};
    }

    result.steps.resize(result.values.size());
    for (std::size_t index = 0; index < result.values.size(); ++index) {
        const Range& range = result.ranges[index % columns];
        const float step = range.step > 0.0F ? std::round((result.values[index] - range.offset) / range.step) : 0.0F;
        result.steps[index] = static_cast<std::uint16_t>(step);
    }

    result.values.clear();
    result.values.shrink_to_fit();
    result.storage = Storage::Quantized;
    return result;
}

Track3D Track3D::relativeTo(std::span<const float> reference) const {
    if (reference.size() != components) {
        std::string message = std::format("a reference of {} holds {} numbers, while {} were given.", describe(), components, reference.size());
        message.front() = 'A';
        throw std::invalid_argument(message);
    }

    std::vector<float> keyValues = getValues();
    std::vector<float> keyHandles = handles;
    if (type == Type::Boolean || type == Type::String) {
        return withKeys(times, std::move(keyValues), std::move(keyHandles), strings, interpolation);
    }

    // A rotation turns by the inverse of the reference from the left, which the tangents of a cubic spline follow since the turn is linear.
    if (type == Type::Quat) {
        const math::Quat inverse = math::Quat{reference[0], reference[1], reference[2], reference[3]}.getNormalized().getInverse();
        for (std::size_t index = 0; index < keyValues.size(); index += 4) {
            const math::Quat turned = inverse * math::Quat{keyValues[index], keyValues[index + 1], keyValues[index + 2], keyValues[index + 3]};
            keyValues[index] = turned.x;
            keyValues[index + 1] = turned.y;
            keyValues[index + 2] = turned.z;
            keyValues[index + 3] = turned.w;
        }

        return withKeys(times, std::move(keyValues), {}, {}, interpolation);
    }

    // A scale divides every value, tangent and handle, while other values subtract the reference from their values alone, whose tangents and handles keep their slopes.
    if (property == "scale") {
        for (std::size_t component = 0; component < components; ++component) {
            if (reference[component] == 0.0F) {
                std::string message = std::format("{} cannot become relative to a reference scale of zero.", describe());
                message.front() = 'T';
                throw std::invalid_argument(message);
            }
        }

        for (std::size_t index = 0; index < keyValues.size(); ++index) {
            keyValues[index] /= reference[index % components];
        }

        for (std::size_t index = 0; index < keyHandles.size(); index += 2) {
            keyHandles[index + 1] /= reference[(index / 4) % components];
        }

        return withKeys(times, std::move(keyValues), std::move(keyHandles), {}, interpolation);
    }

    for (std::size_t key = 0; key < times.size(); ++key) {
        for (std::size_t component = 0; component < components; ++component) {
            keyValues[getValueIndex(key, Slot::Value, component)] -= reference[component];
        }
    }

    return withKeys(times, std::move(keyValues), std::move(keyHandles), {}, interpolation);
}

} // namespace haylen::animation3d
