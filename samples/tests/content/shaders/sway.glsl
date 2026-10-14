// Sways each sprite of a batch around its base by the phase and strength of its own sprite data, and brightens it by the third number, so every sprite of one draw call moves on its own.
@include haylen/material.glsl

@fs sway_fs
@include_block haylen_fragment
layout(binding=1) uniform sway_params {
    float time;
};

void main() {
    float height = (uv.y - haylen_source.y) / max(haylen_source.w - haylen_source.y, 0.0001);
    float bend = sin(time * 2.0 + haylen_data.x) * haylen_data.y * (1.0 - height);
    vec4 base = haylen_base(uv + vec2(bend * (haylen_source.z - haylen_source.x), 0.0));
    haylen_output(vec4(base.rgb * (1.0 + haylen_data.z), base.a));
}
@end

@program sway haylen_vs sway_fs
