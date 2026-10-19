// Runs the colors of the spectrum across a sprite in bands that move, keeping its light and shade.
@include haylen/material.glsl
@include common.glsl

@fs spectrum_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform spectrum_params {
    float bands;
    float speed;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    vec3 hue = library_hue(local.x * bands + local.y * bands * 0.5 - time * speed * 0.2);
    haylen_output(vec4(mix(base.rgb, hue * (library_luminance(base.rgb) * 0.6 + 0.5), 0.75), base.a));
}
@end

@program spectrum haylen_vs spectrum_fs
