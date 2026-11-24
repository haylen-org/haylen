#include "math/NurbsBasis.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>

namespace haylen::math {

std::vector<float> NurbsBasis::makeClampedKnots(std::size_t count, int degree) {
    const auto inner = static_cast<std::ptrdiff_t>(count) - degree;
    std::vector<float> knots(static_cast<std::size_t>(degree) + 1, 0.0F);
    for (std::ptrdiff_t knot = 1; knot < inner; ++knot) {
        knots.push_back(static_cast<float>(knot));
    }

    knots.insert(knots.end(), static_cast<std::size_t>(degree) + 1, static_cast<float>(inner));
    return knots;
}

void NurbsBasis::check(std::string_view subject, std::size_t count, int degree, std::span<const float> knots) {
    if (degree < 1 || degree > kMaxDegree) {
        throw std::invalid_argument(std::format("The {} needs a degree from 1 to 9.", subject));
    }

    if (count < static_cast<std::size_t>(degree) + 1) {
        throw std::invalid_argument(std::format("The {} needs at least one point more than its degree.", subject));
    }

    if (knots.size() != count + static_cast<std::size_t>(degree) + 1) {
        throw std::invalid_argument(std::format("The {} needs as many knots as its points plus its degree plus 1.", subject));
    }

    if (std::ranges::any_of(knots, [](float knot) { return !std::isfinite(knot); }) || !std::ranges::is_sorted(knots)) {
        throw std::invalid_argument(std::format("The knots of the {} must be finite numbers that never decrease.", subject));
    }

    const float low = knots[static_cast<std::size_t>(degree)];
    const float high = knots[count];
    if (!(low < high)) {
        throw std::invalid_argument(std::format("The knots of the {} must leave a range between the knot at its degree and the knot at its point count.", subject));
    }

    // A knot inside the range that repeats more than the degree would break the piece apart.
    for (std::size_t index = 0; index < knots.size();) {
        const auto end = static_cast<std::size_t>(std::upper_bound(knots.begin() + static_cast<std::ptrdiff_t>(index), knots.end(), knots[index]) - knots.begin());
        if (knots[index] > low && knots[index] < high && end - index > static_cast<std::size_t>(degree)) {
            throw std::invalid_argument(std::format("A knot inside the {} repeats at most as many times as its degree.", subject));
        }

        index = end;
    }
}

std::vector<float> NurbsBasis::checkWeights(std::string_view subject, std::vector<float> weights, std::size_t count) {
    if (weights.empty()) {
        return std::vector<float>(count, 1.0F);
    }

    if (weights.size() != count) {
        throw std::invalid_argument(std::format("The {} needs one weight for each point.", subject));
    }

    if (std::ranges::any_of(weights, [](float weight) { return !(weight > 0.0F) || !std::isfinite(weight); })) {
        throw std::invalid_argument(std::format("The weights of the {} must be finite numbers above 0.", subject));
    }

    return weights;
}

float NurbsBasis::toKnot(std::span<const float> knots, int degree, std::size_t count, float t) noexcept {
    const float low = knots[static_cast<std::size_t>(degree)];
    const float high = knots[count];
    return low + (high - low) * std::clamp(t, 0.0F, 1.0F);
}

std::size_t NurbsBasis::findSpan(std::span<const float> knots, int degree, std::size_t count, float u) noexcept {
    const auto first = knots.begin() + degree;
    const auto last = knots.begin() + static_cast<std::ptrdiff_t>(count) + 1;
    auto span = static_cast<std::size_t>(std::upper_bound(first, last, u) - knots.begin()) - 1;
    span = std::clamp(span, static_cast<std::size_t>(degree), count - 1);
    while (span > static_cast<std::size_t>(degree) && knots[span] == knots[span + 1]) {
        --span;
    }

    return span;
}

// The table holds the basis functions of every degree below the top in its upper triangle and the differences of the knots in its lower one, which the derivatives reuse.
void NurbsBasis::evaluate(std::span<const float> knots, int degree, std::size_t span, float u, std::size_t orders, Values& values) noexcept {
    const auto p = static_cast<std::size_t>(degree);
    std::array<std::array<double, kMaxDegree + 1>, kMaxDegree + 1> table{};
    std::array<double, kMaxDegree + 1> left{};
    std::array<double, kMaxDegree + 1> right{};
    table[0][0] = 1.0;
    for (std::size_t j = 1; j <= p; ++j) {
        left[j] = static_cast<double>(u) - knots[span + 1 - j];
        right[j] = static_cast<double>(knots[span + j]) - u;
        double saved = 0.0;
        for (std::size_t r = 0; r < j; ++r) {
            table[j][r] = right[r + 1] + left[j - r];
            const double share = table[r][j - 1] / table[j][r];
            table[r][j] = saved + right[r + 1] * share;
            saved = left[j - r] * share;
        }

        table[j][j] = saved;
    }

    values = {};
    for (std::size_t j = 0; j <= p; ++j) {
        values[0][j] = table[j][p];
    }

    const std::size_t highest = std::min(orders, p);
    std::array<std::array<double, kMaxDegree + 1>, 2> rows{};
    for (std::size_t r = 0; r <= p; ++r) {
        std::size_t current = 0;
        std::size_t next = 1;
        rows[0][0] = 1.0;
        for (std::size_t k = 1; k <= highest; ++k) {
            double derivative = 0.0;
            const auto rk = static_cast<std::ptrdiff_t>(r) - static_cast<std::ptrdiff_t>(k);
            const auto pk = static_cast<std::size_t>(p - k);
            if (rk >= 0) {
                rows[next][0] = rows[current][0] / table[pk + 1][static_cast<std::size_t>(rk)];
                derivative = rows[next][0] * table[static_cast<std::size_t>(rk)][pk];
            }

            const std::ptrdiff_t from = rk >= -1 ? 1 : -rk;
            const std::ptrdiff_t to = static_cast<std::ptrdiff_t>(r) - 1 <= static_cast<std::ptrdiff_t>(pk) ? static_cast<std::ptrdiff_t>(k) - 1 : static_cast<std::ptrdiff_t>(p - r);
            for (std::ptrdiff_t j = from; j <= to; ++j) {
                const auto column = static_cast<std::size_t>(j);
                const auto knot = static_cast<std::size_t>(rk + j);
                rows[next][column] = (rows[current][column] - rows[current][column - 1]) / table[pk + 1][knot];
                derivative += rows[next][column] * table[knot][pk];
            }

            if (r <= pk) {
                rows[next][k] = -rows[current][k - 1] / table[pk + 1][r];
                derivative += rows[next][k] * table[r][pk];
            }

            values[k][r] = derivative;
            std::swap(current, next);
        }
    }

    double factor = static_cast<double>(p);
    for (std::size_t k = 1; k <= highest; ++k) {
        for (std::size_t j = 0; j <= p; ++j) {
            values[k][j] *= factor;
        }

        factor *= static_cast<double>(p - k);
    }
}

} // namespace haylen::math
