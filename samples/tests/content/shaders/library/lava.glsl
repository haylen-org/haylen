// Fills a rectangle with flowing lava: a dark crust broken by glowing cracks that move.
@include haylen/material.glsl
@include common.glsl

@fs lava_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform lava_params {
    vec4 crust_color;
    vec4 glow_color;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv) * 4.0;
    float flow = library_fbm(local + vec2(time * speed * 0.3, time * speed * 0.2));
    float cracks = 1.0 - smoothstep(0.0, 0.08, abs(library_fbm(local * 1.7 - vec2(time * speed * 0.15)) - 0.5));
    float heat = clamp(cracks + (flow - 0.4) * 0.8, 0.0, 1.0);
    haylen_output(vec4(mix(crust_color.rgb, glow_color.rgb, heat) + vec3(1.0, 0.9, 0.5) * pow(heat, 4.0) * 0.6, color.a));
}
@end

@program lava haylen_vs lava_fs
