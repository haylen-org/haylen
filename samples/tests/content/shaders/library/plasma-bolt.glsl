// Turns a shape into a ball of plasma: turbulent energy that churns and streams back from its front, a white-hot heart, a rim of the outer color and a flickering halo around it, for magic projectiles, power cores and charged shots drawn with the additive blend.
@include haylen/material.glsl
@include common.glsl

@fs plasmaBolt_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform plasmaBolt_params {
    vec4 inner_color;
    vec4 outer_color;
    vec2 texel;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float flow = time * speed;

    // The body of the bolt is the soft shape of the image, and the energy inside churns as it streams back from the front.
    float body = library_glow(uv, texel, 10.0);
    vec2 point = local * vec2(3.4, 1.6) + vec2(flow * 2.4, 0.0);
    float warp = library_fbm(point * 0.8 + vec2(flow * 0.7, -flow * 0.4));
    float churn = library_fbm(point + warp * 2.2);
    float heat = clamp(pow(body, 1.4) * (0.15 + 1.1 * churn), 0.0, 1.0);

    // A halo that flickers with the plasma spreads around the bolt.
    float halo = library_glow(uv, texel, 34.0) * (0.75 + 0.5 * library_noise(vec2(local.x * 9.0 + flow * 3.0, flow * 2.0)));

    vec3 plasma = mix(outer_color.rgb, inner_color.rgb, smoothstep(0.5, 0.95, heat)) * heat * 1.25;
    vec3 light = plasma + vec3(1.0) * pow(heat, 9.0) * 0.7 + outer_color.rgb * pow(halo, 0.7) * 0.8;
    float amount = clamp(max(max(light.r, light.g), light.b), 0.0, 1.0);
    haylen_output(vec4(light / max(amount, 0.0001), amount * inner_color.a));
}
@end

@program plasmaBolt haylen_vs plasmaBolt_fs
