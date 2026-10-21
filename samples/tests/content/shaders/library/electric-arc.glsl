// Shades a stroke as an electric arc: a jagged bolt of lightning pinned at both ends that jumps to a new shape many times a second, a thinner fork that flashes beside it, a white core, a glow of the arc color and a flicker of brightness, for tesla coils, chain lightning and shocks drawn with the additive blend.
@include haylen/material.glsl
@include common.glsl

@fs electricArc_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform electricArc_params {
    vec4 arc_color;
    float jitter;
    float thickness;
    float time;
};

// A random offset at each of `count` points along the stroke joined by straight segments, which gives the sharp kinks of lightning.
float electricArc_segments(float along, float count, float seed) {
    float point = along * count;
    float index = floor(point);
    return mix(library_hash(vec2(index, seed)), library_hash(vec2(index + 1.0, seed)), fract(point)) - 0.5;
}

// How far from the middle of the stroke a bolt runs at a point along it, made of kinks of large, middle and small size, with the small ones scaled by `detail`, which is zero at both ends so the bolt stays pinned to them.
float electricArc_path(float along, float seed, float detail) {
    float pinned = sin(along * 3.14159265);
    float large = electricArc_segments(along, 5.0, seed);
    float middle = electricArc_segments(along, 17.0, seed + 13.0);
    float small = electricArc_segments(along, 52.0, seed + 29.0);
    return (large + middle * 0.55 + small * 0.3 * detail) * pinned;
}

void main() {
    vec2 local = library_local(uv);
    float along = clamp(local.x, 0.0, 1.0);
    float side = local.y - 0.5;

    // The bolt jumps to a new shape at irregular moments, and each shape flashes bright and settles.
    float tick = time * 16.0;
    float jump = floor(tick + library_hash(vec2(floor(tick), 3.0)) * 0.6);
    float flash = 0.6 + 0.4 * library_hash(vec2(jump, 7.0)) + 0.5 * exp(-fract(tick) * 6.0);

    // The core follows every kink, while the wide glow follows the bolt without its smallest kinks, so it stays smooth across them.
    float bolt = abs(side - electricArc_path(along, jump, 1.0) * jitter);
    float smoothed = abs(side - electricArc_path(along, jump, 0.0) * jitter);
    float core = 1.0 - smoothstep(thickness * 0.15, thickness * 0.45, bolt);
    float window = (1.0 - smoothstep(0.3, 0.5, abs(side))) * smoothstep(0.0, 0.02, along) * smoothstep(1.0, 0.98, along);
    float glow = (exp(-bolt / max(thickness, 0.005) * 0.6) * 1.1 + exp(-smoothed * 5.0) * 0.6) * window;

    // A thinner fork follows another path and shows only on some of the jumps.
    float forked = step(0.45, library_hash(vec2(jump, 11.0)));
    float fork = abs(side - electricArc_path(along, jump + 41.0, 1.0) * jitter * 1.3);
    float forkLight = forked * ((1.0 - smoothstep(thickness * 0.08, thickness * 0.25, fork)) * 0.8 + exp(-fork / max(thickness, 0.005) * 1.6) * 0.5);

    vec3 light = (vec3(1.0) * core * 1.4 + arc_color.rgb * (glow + forkLight)) * flash + vec3(1.0) * forkLight * 0.4;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * color.a * arc_color.a));
}
@end

@program electricArc haylen_vs electricArc_fs
