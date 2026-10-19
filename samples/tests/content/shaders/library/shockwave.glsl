// Pushes an image outward in a round ring that grows with the progress and fades as it spreads, with a bright front, for explosions and stomps.
@include haylen/material.glsl
@include common.glsl

@fs shockwave_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform shockwave_params {
    vec2 texel;
    float progress;
    float width;
    float strength;
};

void main() {
    vec2 local = library_local(uv);
    vec2 aspect = vec2(texel.y / texel.x, 1.0) / max(texel.y / texel.x, 1.0);
    vec2 offset = (local - 0.5) * aspect;
    float distance = length(offset);
    float edge = (distance - progress * 0.75) / width;
    float fade = 1.0 - progress;
    float band = (1.0 - smoothstep(0.0, 1.0, abs(edge))) * fade;
    float push = edge * pow(clamp(1.0 - abs(edge), 0.0, 1.0), 2.0) * fade;
    vec2 direction = offset / max(distance, 0.0001) / aspect;
    vec4 base = haylen_base(haylen_inside(library_texture_point(local - direction * push * strength * width * 5.0)));
    haylen_output(vec4(base.rgb + band * (0.15 + 0.35 * smoothstep(0.0, -1.0, edge)), base.a));
}
@end

@program shockwave haylen_vs shockwave_fs
