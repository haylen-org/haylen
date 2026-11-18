#include "haylen/math/KalmanFilter.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>

#include "haylen/math/SignalFilterHelper.hpp"

namespace haylen::math {

KalmanFilter::KalmanFilter(float process, float measurement, int count) : processNoise(process), measurementNoise(measurement), dimensions(count) {
    if (!(process > 0.0F) || !std::isfinite(process) || !(measurement > 0.0F) || !std::isfinite(measurement)) {
        throw std::invalid_argument("The process noise and the measurement noise of a Kalman filter are positive numbers.");
    }

    SignalFilterHelper::checkDimensions(count);
}

// The estimate grows less certain by the process noise between updates, and the gain moves it toward the measurement by the share of the uncertainty that the estimate holds.
void KalmanFilter::update(std::span<float> values) {
    const std::size_t used = std::min(values.size(), static_cast<std::size_t>(dimensions));
    SignalFilterHelper::checkValues(values.first(used));

    if (!started) {
        started = true;

        for (std::size_t index = 0; index < used; ++index) {
            estimate[index] = values[index];
            covariance[index] = measurementNoise;
        }

        return;
    }

    for (std::size_t index = 0; index < used; ++index) {
        const float predicted = covariance[index] + processNoise;
        const float gain = predicted / (predicted + measurementNoise);
        estimate[index] += gain * (values[index] - estimate[index]);
        covariance[index] = (1.0F - gain) * predicted;
        values[index] = estimate[index];
    }
}

float KalmanFilter::update(float value) {
    update(std::span(&value, 1));
    return value;
}

void KalmanFilter::reset() noexcept {
    started = false;
    covariance.fill(0.0F);
}

} // namespace haylen::math
