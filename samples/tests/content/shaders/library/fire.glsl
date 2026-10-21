// Draws a flame that rises over a rectangle: tongues of fire that lick upward from a round base, shaped by noise that streams up and sways, white-hot at the root, hot in the body and cooling to the tips, with sparks of heat that break off near the top, drawn with the additive blend.
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
    float rise = 1.0 - local.y;
    float flow = time * speed;

    // Noise that streams up and sways bends the flame into tongues.
    vec2 point = vec2(local.x * 3.2, rise * 2.6 - flow * 1.5);
    float sway = (library_fbm(vec2(rise * 1.5 - flow * 0.5, flow * 0.3)) - 0.5) * 0.25 * rise;
    float turbulence = library_fbm(point + vec2(library_fbm(point * 1.7 + flow * 0.4) * 1.2, 0.0));

    // The flame is widest just above its round base and narrows to tips that the turbulence tears apart.
    float centered = abs(local.x - 0.5 - sway) * 2.0;
    float base = sqrt(clamp(rise / 0.2, 0.0, 1.0));
    float profile = mix(0.85, 0.1, pow(rise, 0.8)) * base;
    float body = smoothstep(profile, profile * 0.3, centered + (turbulence - 0.5) * 0.8 * (0.25 + rise));
    float heat = clamp(body * (1.1 - rise * 0.85) * (0.65 + turbulence * 0.6) * intensity, 0.0, 1.5);

    // Small sparks of heat break off and drift up above the flame.
    vec2 spark = vec2(local.x * 22.0, rise * 16.0 - flow * 4.0);
    float ember = step(0.97, library_hash(floor(spark))) * (1.0 - smoothstep(0.08, 0.2, length(fract(spark) - 0.5)));
    float sparks = ember * smoothstep(0.3, 0.6, rise) * (1.0 - rise) * 2.0;

    vec3 shade = mix(cool_color.rgb, hot_color.rgb, smoothstep(0.25, 0.85, heat));
    vec3 light = shade * heat + vec3(1.0, 0.95, 0.85) * smoothstep(1.0, 1.45, heat) + hot_color.rgb * sparks;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * hot_color.a));
}
@end

@program fire haylen_vs fire_fs
