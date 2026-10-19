// Shows an image as an x-ray: the light inverted in a cold blue, with a glow.
@include haylen/material.glsl
@include common.glsl

@fs xRay_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform xRay_params {
    float glow;
};

void main() {
    vec4 base = haylen_base(uv);
    float density = 1.0 - library_luminance(base.rgb);
    haylen_output(vec4(vec3(0.6, 0.85, 1.0) * pow(density, 1.5) * (1.0 + glow), base.a));
}
@end

@program xRay haylen_vs xRay_fs
