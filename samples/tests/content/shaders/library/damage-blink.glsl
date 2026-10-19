// Blinks a sprite between its colors and the blink color at a rate, for the moments a character cannot be hit.
@include haylen/material.glsl
@include common.glsl

@fs damageBlink_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform damageBlink_params {
    vec4 blink_color;
    float rate;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    float blink = step(0.5, fract(time * rate));
    haylen_output(vec4(mix(base.rgb, blink_color.rgb, blink * blink_color.a), base.a));
}
@end

@program damageBlink haylen_vs damageBlink_fs
