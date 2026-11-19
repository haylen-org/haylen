#pragma once

#include <gtest/gtest.h>

#include "haylen/math/Vec2.hpp"

namespace haylen::test {

// Compares the vectors that geometry computes, within the rounding that its floats keep.
class VectorExpectations final {
  public:
    static constexpr float kTolerance = 1.0e-3F;

    static void expectNear(math::Vec2 actual, math::Vec2 expected) {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
    }
};

} // namespace haylen::test
