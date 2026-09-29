// Turns the draw toward a flat color by an amount, the white blink of a character that takes a hit.
@include haylen/material.glsl

@fs flash_fs
@include_block haylen_fragment
layout(binding=1) uniform flash_params {
    vec4 flash_color;
    float amount;
};

void main() {
    vec4 base = haylen_base(uv);
    haylen_output(vec4(mix(base.rgb, flash_color.rgb, amount), base.a));
}
@end

@program flash haylen_vs flash_fs
