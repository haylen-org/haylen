// Teleports a sprite in from the bottom up: below a bright scan line it is whole, with a charge of the beam color just under the line, above the line it is still a hologram of fragments that stream up and fade, and a column of light stands over it, so it takes the sprite apart again as the progress falls.
@include haylen/material.glsl
@include common.glsl

@fs teleport_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform teleport_params {
    vec4 beam_color;
    float progress;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float front = 1.0 - progress;
    vec4 base = haylen_base(uv);

    // Below the line the sprite is built, and the part just under the line still glows with the beam.
    float built = step(front, local.y);
    float charge = exp(-max(local.y - front, 0.0) * 14.0);
    vec3 whole = mix(base.rgb, beam_color.rgb + vec3(0.35), charge * 0.75);

    // Above the line the sprite is a hologram of columns of fragments, lifted further and thinned out the higher they are.
    float column = floor(local.x * 110.0);
    float height = max(front - local.y, 0.0);
    float lift = height * (0.4 + 0.6 * library_hash(vec2(column, 1.0)));
    vec4 lifted = haylen_base(library_texture_point(vec2(local.x, local.y + lift)));
    float fragments = step(library_hash(vec2(column, floor(local.y * 160.0 + time * 40.0 * (0.5 + library_hash(vec2(column, 2.0)))))), 1.0 - height * 3.5);
    float scan = 0.75 + 0.25 * sin(local.y * 700.0 - time * 20.0);
    float hologram = lifted.a * fragments * scan * (1.0 - smoothstep(0.0, 0.3, height)) * 0.7;

    // The scan line spans the width of the sprite at its height, and a column of light stands over the sprite.
    float widthAtLine = haylen_base(library_texture_point(vec2(local.x, front))).a;
    float sides = smoothstep(0.0, 0.2, local.x) * smoothstep(1.0, 0.8, local.x);
    float line = exp(-abs(local.y - front) * 120.0) * (0.15 + 0.85 * widthAtLine) * sides;
    float beam = exp(-pow((local.x - 0.5) / 0.22, 2.0)) * smoothstep(0.0, 0.25, local.y) * smoothstep(1.0, 0.8, local.y) * (0.12 + 0.08 * library_noise(vec2(local.x * 30.0, local.y * 4.0 + time * 6.0)));

    vec3 light = beam_color.rgb + vec3(0.5);
    vec4 sprite = vec4(whole, base.a * built);
    vec4 glow = vec4(light, clamp(hologram * 0.85 + line * 1.4 + beam, 0.0, 1.0));
    float alpha = sprite.a + glow.a * (1.0 - sprite.a);
    haylen_output(vec4(mix(glow.rgb, sprite.rgb, sprite.a / max(alpha, 0.0001)), alpha));
}
@end

@program teleport haylen_vs teleport_fs
