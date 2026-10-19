// Prints an image as dots of ink on paper, larger where it is darker, like a comic book.
@include haylen/material.glsl
@include common.glsl

@fs halftone_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform halftone_params {
    float cell;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 center = fract(gl_FragCoord.xy / cell) - 0.5;
    float radius = sqrt(1.0 - library_luminance(base.rgb)) * 0.6;
    float ink = 1.0 - smoothstep(radius - 0.08, radius + 0.08, length(center));
    haylen_output(vec4(mix(vec3(0.97, 0.95, 0.9), base.rgb * 0.85, ink), base.a));
}
@end

@program halftone haylen_vs halftone_fs
