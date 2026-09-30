#include <gtest/gtest.h>

#include "2d/graphics/RendererState.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/math/Color.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics2d {

// Blends one pixel on the CPU the way a draw does: the shader writes the color of the draw, multiplied by its alpha when the blend mode expects that, and the pipeline blend of the mode mixes it with the target.
class RendererStateTest : public ::testing::Test {
  protected:
    using Mode = graphics::BlendMode::Type;

    [[nodiscard]] static math::Color blend(Mode mode, math::Color color, math::Color target) {
        const float written = RendererState::expectsPremultiplied(mode) ? color.a : 1.0F;
        const math::Color source{color.r * written, color.g * written, color.b * written, color.a};
        const sg_blend_state state = RendererState::blendState(mode);
        if (!state.enabled) {
            return source;
        }
        return {mix(state, source.r, source.a, target.r), mix(state, source.g, source.a, target.g), mix(state, source.b, source.a, target.b), target.a};
    }

  private:
    [[nodiscard]] static float mix(const sg_blend_state& state, float source, float sourceAlpha, float target) {
        return source * factor(state.src_factor_rgb, true, source, sourceAlpha, target) + target * factor(state.dst_factor_rgb, false, source, sourceAlpha, target);
    }

    [[nodiscard]] static float factor(sg_blend_factor value, bool isSource, float source, float sourceAlpha, float target) {
        switch (value) {
        case _SG_BLENDFACTOR_DEFAULT:
            return isSource ? 1.0F : 0.0F;
        case SG_BLENDFACTOR_ZERO:
            return 0.0F;
        case SG_BLENDFACTOR_ONE:
            return 1.0F;
        case SG_BLENDFACTOR_SRC_COLOR:
            return source;
        case SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR:
            return 1.0F - source;
        case SG_BLENDFACTOR_SRC_ALPHA:
            return sourceAlpha;
        case SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA:
            return 1.0F - sourceAlpha;
        case SG_BLENDFACTOR_DST_COLOR:
            return target;
        case SG_BLENDFACTOR_ONE_MINUS_DST_COLOR:
            return 1.0F - target;
        default:
            ADD_FAILURE() << "The blend factor " << static_cast<int>(value) << " is not emulated.";
            return 0.0F;
        }
    }
};

TEST_F(RendererStateTest, BlendsTheStraightColorsOfDrawsInEveryModeButPremultiplied) {
    const math::Color target{0.6F, 0.4F, 0.2F, 1.0F};
    const math::Color color{0.5F, 1.0F, 0.0F, 0.5F};

    // A half transparent draw has half the effect of an opaque one, whatever the mode.
    const math::Color alpha = blend(Mode::Alpha, color, target);
    EXPECT_FLOAT_EQ(alpha.r, 0.55F);
    EXPECT_FLOAT_EQ(alpha.g, 0.7F);
    EXPECT_FLOAT_EQ(alpha.b, 0.1F);
    const math::Color additive = blend(Mode::Additive, color, target);
    EXPECT_FLOAT_EQ(additive.r, 0.85F);
    EXPECT_FLOAT_EQ(additive.g, 0.9F);
    EXPECT_FLOAT_EQ(additive.b, 0.2F);
    const math::Color multiply = blend(Mode::Multiply, color, target);
    EXPECT_FLOAT_EQ(multiply.r, 0.45F);
    EXPECT_FLOAT_EQ(multiply.g, 0.4F);
    EXPECT_FLOAT_EQ(multiply.b, 0.1F);
    const math::Color screen = blend(Mode::Screen, color, target);
    EXPECT_FLOAT_EQ(screen.r, 0.7F);
    EXPECT_FLOAT_EQ(screen.g, 0.7F);
    EXPECT_FLOAT_EQ(screen.b, 0.2F);

    // A premultiplied draw of the same color matches the alpha draw of its straight color.
    const math::Color premultiplied = blend(Mode::Premultiplied, {0.25F, 0.5F, 0.0F, 0.5F}, target);
    EXPECT_FLOAT_EQ(premultiplied.r, alpha.r);
    EXPECT_FLOAT_EQ(premultiplied.g, alpha.g);
    EXPECT_FLOAT_EQ(premultiplied.b, alpha.b);

    // Opaque multiply and screen draws are the classic formulas, and a transparent draw leaves the target alone.
    const math::Color solid{0.5F, 1.0F, 0.0F, 1.0F};
    EXPECT_FLOAT_EQ(blend(Mode::Multiply, solid, target).r, 0.3F);
    EXPECT_FLOAT_EQ(blend(Mode::Screen, solid, target).r, 0.8F);
    for (const Mode mode : {Mode::Alpha, Mode::Additive, Mode::Multiply, Mode::Screen, Mode::Premultiplied}) {
        const math::Color untouched = blend(mode, {0.0F, 0.0F, 0.0F, 0.0F}, target);
        EXPECT_FLOAT_EQ(untouched.r, target.r);
        EXPECT_FLOAT_EQ(untouched.g, target.g);
        EXPECT_FLOAT_EQ(untouched.b, target.b);
    }
    EXPECT_FLOAT_EQ(blend(Mode::Opaque, color, target).r, color.r);
}

} // namespace haylen::graphics2d
