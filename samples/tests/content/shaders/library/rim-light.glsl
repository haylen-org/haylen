// Lights the edges of a sprite that face a direction, as if a light stood on that side.
@include haylen/material.glsl
@include common.glsl

@fs rimLight_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform rimLight_params {
    vec4 rim_color;
    vec2 texel;
    float angle;
    float width;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 direction = vec2(cos(angle), sin(angle));
    float beyond = haylen_texture(haylen_inside(uv + direction * texel * width)).a;
    haylen_output(vec4(mix(base.rgb, rim_color.rgb, base.a * (1.0 - beyond) * rim_color.a), base.a));
}
@end

@program rimLight haylen_vs rimLight_fs
