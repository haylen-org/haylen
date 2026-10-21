// A flickering hologram for the additive blend: the sprite in the light of the tint, bright along its rim, with scanlines that roll over the screen and a slight color split.
@include haylen/material.glsl

@fs hologram_fs
@include_block haylen_fragment
layout(binding=1) uniform hologram_params {
    vec4 tint;
    vec2 texel;
    float time;
    float lines;
};

void main() {
    float split = 0.004 + 0.003 * sin(time * 13.0);
    vec4 base = haylen_base(uv);
    float red = haylen_base(uv + vec2(split, 0.0)).a;
    float blue = haylen_base(uv - vec2(split, 0.0)).a;
    // The rim is where the shape ends within a few texels, which glows like the edge of a projection.
    vec2 reach = texel * 3.0;
    float inner = min(min(haylen_base(uv + vec2(reach.x, 0.0)).a, haylen_base(uv - vec2(reach.x, 0.0)).a), min(haylen_base(uv + vec2(0.0, reach.y)).a, haylen_base(uv - vec2(0.0, reach.y)).a));
    float rim = clamp(base.a - inner, 0.0, 1.0);
    float glow = dot(base.rgb, vec3(0.3, 0.59, 0.11)) * 0.7 + 0.3;
    float scan = 0.7 + 0.3 * sin(gl_FragCoord.y * lines + time * 8.0);
    float flicker = 0.88 + 0.12 * sin(time * 47.0) * sin(time * 23.0);
    vec3 light = vec3(tint.r * red, tint.g * base.a, tint.b * blue) * glow * scan + tint.rgb * rim * 1.6;
    float alpha = max(base.a, max(red, blue)) * flicker;
    haylen_output(vec4(light * flicker, alpha));
}
@end

@program hologram haylen_vs hologram_fs
