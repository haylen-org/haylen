// Turns a sprite into polished gold with a shine that sweeps across it, for trophies and rare items.
@include haylen/material.glsl
@include common.glsl

@fs gold_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform gold_params {
    float amount;
    float shine_speed;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    vec3 gold = mix(vec3(0.45, 0.28, 0.05), vec3(1.0, 0.86, 0.4), library_luminance(base.rgb));
    float band = fract(local.x + local.y * 0.5 - time * shine_speed * 0.4);
    float shine = exp(-pow((band - 0.5) * 12.0, 2.0));
    haylen_output(vec4(mix(base.rgb, gold + shine * 0.6, amount), base.a));
}
@end

@program gold haylen_vs gold_fs
