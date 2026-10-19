// Makes a sprite a translucent, bluish ghost that wobbles and flickers.
@include haylen/material.glsl
@include common.glsl

@fs ghost_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform ghost_params {
    float opacity;
    float wobble;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    local.x += sin(local.y * 12.0 + time * 3.0) * wobble * 0.02;
    vec4 base = haylen_base(haylen_inside(library_texture_point(local)));
    float flicker = 0.85 + 0.15 * sin(time * 9.0);
    haylen_output(vec4(mix(base.rgb, vec3(0.7, 0.85, 1.0), 0.5), base.a * opacity * flicker));
}
@end

@program ghost haylen_vs ghost_fs
