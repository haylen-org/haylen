#pragma once

#include <span>
#include <string_view>

#include "math/ScalarFloat4.hpp"

// The batch math computes four floats at once with the vector instructions that every processor of an architecture has, which the compiler of each target names: SSE2 on x64, Neon on ARM and the SIMD of WebAssembly, which the web build turns on. A processor without them computes the lanes one at a time.
#if defined(__SSE2__) || defined(_M_X64)
#include <emmintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

namespace haylen::math {

#if defined(__SSE2__) || defined(_M_X64)

// Four floats in one register of SSE2.
class Float4 final {
  public:
    using Value = __m128;

    static constexpr std::string_view kInstructions = "SSE2";

    [[nodiscard]] static Value load(std::span<const float, 4> values) noexcept {
        return _mm_loadu_ps(values.data());
    }

    static void store(Value value, std::span<float, 4> values) noexcept {
        _mm_storeu_ps(values.data(), value);
    }

    [[nodiscard]] static Value splat(float value) noexcept {
        return _mm_set1_ps(value);
    }

    [[nodiscard]] static Value add(Value lhs, Value rhs) noexcept {
        return _mm_add_ps(lhs, rhs);
    }

    [[nodiscard]] static Value subtract(Value lhs, Value rhs) noexcept {
        return _mm_sub_ps(lhs, rhs);
    }

    [[nodiscard]] static Value multiply(Value lhs, Value rhs) noexcept {
        return _mm_mul_ps(lhs, rhs);
    }

    // Clears the sign bit of every lane.
    [[nodiscard]] static Value absolute(Value value) noexcept {
        return _mm_andnot_ps(_mm_set1_ps(-0.0F), value);
    }

    [[nodiscard]] static bool anyLess(Value lhs, Value rhs) noexcept {
        return _mm_movemask_ps(_mm_cmplt_ps(lhs, rhs)) != 0;
    }
};

#elif defined(__ARM_NEON)

// Four floats in one register of Neon.
class Float4 final {
  public:
    using Value = float32x4_t;

    static constexpr std::string_view kInstructions = "Neon";

    [[nodiscard]] static Value load(std::span<const float, 4> values) noexcept {
        return vld1q_f32(values.data());
    }

    static void store(Value value, std::span<float, 4> values) noexcept {
        vst1q_f32(values.data(), value);
    }

    [[nodiscard]] static Value splat(float value) noexcept {
        return vdupq_n_f32(value);
    }

    [[nodiscard]] static Value add(Value lhs, Value rhs) noexcept {
        return vaddq_f32(lhs, rhs);
    }

    [[nodiscard]] static Value subtract(Value lhs, Value rhs) noexcept {
        return vsubq_f32(lhs, rhs);
    }

    [[nodiscard]] static Value multiply(Value lhs, Value rhs) noexcept {
        return vmulq_f32(lhs, rhs);
    }

    [[nodiscard]] static Value absolute(Value value) noexcept {
        return vabsq_f32(value);
    }

    // Folds the four lanes of the comparison into one with two bitwise ors, which ARM processors of 32 and 64 bits both have.
    [[nodiscard]] static bool anyLess(Value lhs, Value rhs) noexcept {
        const uint32x4_t less = vcltq_f32(lhs, rhs);
        const uint32x2_t folded = vorr_u32(vget_low_u32(less), vget_high_u32(less));
        return (vget_lane_u32(folded, 0) | vget_lane_u32(folded, 1)) != 0U;
    }
};

#elif defined(__wasm_simd128__)

// Four floats in one vector of the SIMD of WebAssembly.
class Float4 final {
  public:
    using Value = v128_t;

    static constexpr std::string_view kInstructions = "WebAssembly SIMD";

    [[nodiscard]] static Value load(std::span<const float, 4> values) noexcept {
        return wasm_v128_load(values.data());
    }

    static void store(Value value, std::span<float, 4> values) noexcept {
        wasm_v128_store(values.data(), value);
    }

    [[nodiscard]] static Value splat(float value) noexcept {
        return wasm_f32x4_splat(value);
    }

    [[nodiscard]] static Value add(Value lhs, Value rhs) noexcept {
        return wasm_f32x4_add(lhs, rhs);
    }

    [[nodiscard]] static Value subtract(Value lhs, Value rhs) noexcept {
        return wasm_f32x4_sub(lhs, rhs);
    }

    [[nodiscard]] static Value multiply(Value lhs, Value rhs) noexcept {
        return wasm_f32x4_mul(lhs, rhs);
    }

    [[nodiscard]] static Value absolute(Value value) noexcept {
        return wasm_f32x4_abs(value);
    }

    [[nodiscard]] static bool anyLess(Value lhs, Value rhs) noexcept {
        return wasm_v128_any_true(wasm_f32x4_lt(lhs, rhs));
    }
};

#else

using Float4 = ScalarFloat4;

#endif

} // namespace haylen::math
