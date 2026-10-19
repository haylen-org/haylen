// Draws a flame that rises over a rectangle from noise, hot at its core and cooler toward its tips.
@include haylen/material.glsl
@include common.glsl

@fs fire_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform fire_params {
    vec4 hot_color;
    vec4 cool_color;
    float speed;
    float intensity;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float flow = library_fbm(vec2(local.x * 5.0, local.y * 4.0 + time * speed));
    float column = clamp(1.0 - abs(local.x - 0.5) * 2.6, 0.0, 1.0);
    float base = smoothstep(1.0, 0.9, local.y);
    float heat = clamp((column * local.y * 1.6 - flow * 0.8) * intensity, 0.0, 1.0) * base;
    vec3 shade = mix(cool_color.rgb, hot_color.rgb, heat) + vec3(1.0, 0.95, 0.8) * smoothstep(0.75, 1.0, heat);
    haylen_output(vec4(shade, smoothstep(0.05, 0.3, heat) * color.a));
}
@end

@program fire haylen_vs fire_fs
