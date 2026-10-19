// Sweeps a diagonal band of light across a sprite, for buttons, cards and items that call for a tap.
@include haylen/material.glsl
@include common.glsl

@fs shineSweep_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform shineSweep_params {
    vec4 shine_color;
    float width;
    float speed;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    float position = fract(time * speed * 0.3) * 2.0 - 0.5;
    float shine = 1.0 - smoothstep(0.0, width, abs(local.x + (1.0 - local.y) * 0.4 - position));
    haylen_output(vec4(base.rgb + shine_color.rgb * shine * shine_color.a, base.a));
}
@end

@program shineSweep haylen_vs shineSweep_fs
