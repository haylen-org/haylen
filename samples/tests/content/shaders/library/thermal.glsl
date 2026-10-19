// Shows the light of an image as heat, from cold blues through reds to white.
@include haylen/material.glsl
@include common.glsl

@fs thermal_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform thermal_params {
    float amount;
};

void main() {
    vec4 base = haylen_base(uv);
    float heat = library_luminance(base.rgb);
    vec3 cold = vec3(0.05, 0.0, 0.4);
    vec3 middle = vec3(0.8, 0.0, 0.6);
    vec3 warm = vec3(1.0, 0.4, 0.0);
    vec3 hot = vec3(1.0, 1.0, 0.6);
    vec3 ramp = heat < 0.33 ? mix(cold, middle, heat / 0.33) : (heat < 0.66 ? mix(middle, warm, (heat - 0.33) / 0.33) : mix(warm, hot, (heat - 0.66) / 0.34));
    haylen_output(vec4(mix(base.rgb, ramp, amount), base.a));
}
@end

@program thermal haylen_vs thermal_fs
