#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace haylen::test {

// The materials that the tests of shaders and materials compile, the way apps compile theirs.
class MaterialShaders final {
  public:
    // A material with uniform blocks of every kind of member, floats and integers in blocks of their own, and a texture of its own.
    static constexpr const char* kTintSource = R"(@include haylen/material.glsl

@fs tint_fs
@include_block haylen_fragment
layout(binding=1) uniform tint_params {
    vec4 tint;
    float strength;
    vec2 offset;
    mat4 transform;
    vec4 palette[3];
};
layout(binding=2) uniform tint_extra {
    vec3 glow;
    float weights[3];
};
layout(binding=3) uniform tint_counts {
    int steps;
};
layout(binding=1) uniform texture2D mask_texture;
layout(binding=1) uniform sampler mask_sampler;

void main() {
    vec4 base = haylen_base(uv + offset);
    float mask = texture(sampler2D(mask_texture, mask_sampler), uv).r;
    vec4 moved = transform * vec4(base.rgb, 1.0);
    haylen_output(vec4(mix(base.rgb, moved.rgb * tint.rgb + glow * weights[steps] + palette[steps].rgb, strength * mask), base.a));
}
@end

@program tint haylen_vs tint_fs
)";

    // A material that never samples its texture, so its sprite sampler pairs with no texture in any program.
    static constexpr const char* kGlowSource = R"(@include haylen/material.glsl

@fs glow_fs
@include_block haylen_fragment
layout(binding=1) uniform glow_params {
    vec4 glow;
};

void main() {
    haylen_output(glow * color);
}
@end

@program glow haylen_vs glow_fs
)";

    // A material that samples a cube texture of its own in the direction of its texture coordinates.
    static constexpr const char* kSkySource = R"(@include haylen/material.glsl

@fs sky_fs
@include_block haylen_fragment
layout(binding=1) uniform textureCube sky_texture;
layout(binding=1) uniform sampler sky_sampler;

void main() {
    vec3 direction = vec3(uv * 2.0 - 1.0, 1.0);
    haylen_output(texture(samplerCube(sky_texture, sky_sampler), direction) * color);
}
@end

@program sky haylen_vs sky_fs
)";

    [[nodiscard]] static std::vector<std::uint8_t> compile(const std::string& name, const char* source);
    [[nodiscard]] static const std::vector<std::uint8_t>& tint();
};

} // namespace haylen::test
