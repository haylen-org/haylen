// Fills a shape with swirling plasma between two colors, for magic projectiles and power cores.
@include haylen/material.glsl
@include common.glsl

@fs plasmaBolt_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform plasmaBolt_params {
    vec4 inner_color;
    vec4 outer_color;
    float speed;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv) * 4.0;
    float flow = library_fbm(local + vec2(time * speed, -time * speed * 0.7));
    float swirl = sin((local.x + local.y) * 3.0 + flow * 6.0 + time * speed * 2.0) * 0.5 + 0.5;
    haylen_output(vec4(mix(outer_color.rgb, inner_color.rgb, swirl) * (0.7 + base.a * 0.5), base.a));
}
@end

@program plasmaBolt haylen_vs plasmaBolt_fs
