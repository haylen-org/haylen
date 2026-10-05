// Draws a colored outline around the opaque pixels of a sprite, which needs a transparent border in its image to have room for it.
@include haylen/material.glsl

@fs outline_fs
@include_block haylen_fragment
layout(binding=1) uniform outline_params {
    vec4 outline_color;
    vec2 texel;
    float thickness;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 step = texel * thickness;
    float around = 0.0;
    around = max(around, haylen_texture(uv + vec2(step.x, 0.0)).a);
    around = max(around, haylen_texture(uv - vec2(step.x, 0.0)).a);
    around = max(around, haylen_texture(uv + vec2(0.0, step.y)).a);
    around = max(around, haylen_texture(uv - vec2(0.0, step.y)).a);
    around = max(around, haylen_texture(uv + step).a);
    around = max(around, haylen_texture(uv - step).a);
    around = max(around, haylen_texture(uv + vec2(step.x, -step.y)).a);
    around = max(around, haylen_texture(uv + vec2(-step.x, step.y)).a);
    float border = around * (1.0 - base.a) * outline_color.a;
    haylen_output(vec4(mix(base.rgb, outline_color.rgb, border), max(base.a, border)));
}
@end

@program outline haylen_vs outline_fs
