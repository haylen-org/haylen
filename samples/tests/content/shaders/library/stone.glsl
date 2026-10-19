// Turns a sprite into gray stone with grain, for petrified enemies and statues.
@include haylen/material.glsl
@include common.glsl

@fs stone_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform stone_params {
    float amount;
};

void main() {
    vec4 base = haylen_base(uv);
    float grain = library_fbm(library_local(uv) * 18.0) * 0.35 + 0.75;
    vec3 stone = vec3(library_luminance(base.rgb) * 0.8 + 0.1) * grain * vec3(0.95, 0.93, 0.9);
    haylen_output(vec4(mix(base.rgb, stone, amount), base.a));
}
@end

@program stone haylen_vs stone_fs
