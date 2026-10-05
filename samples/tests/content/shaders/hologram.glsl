// A flickering hologram: tinted, translucent, with scanlines that roll over the screen and a slight color split.
@include haylen/material.glsl

@fs hologram_fs
@include_block haylen_fragment
layout(binding=1) uniform hologram_params {
    vec4 tint;
    float time;
    float lines;
};

void main() {
    float split = 0.004 + 0.003 * sin(time * 13.0);
    vec4 base = haylen_base(uv);
    float red = haylen_base(uv + vec2(split, 0.0)).a;
    float blue = haylen_base(uv - vec2(split, 0.0)).a;
    float glow = dot(base.rgb, vec3(0.3, 0.59, 0.11)) * 0.6 + 0.4;
    float scan = 0.65 + 0.35 * sin(gl_FragCoord.y * lines + time * 8.0);
    float flicker = 0.85 + 0.15 * sin(time * 47.0) * sin(time * 23.0);
    float alpha = max(base.a, max(red, blue)) * scan * flicker * 0.8;
    vec3 shifted = vec3(tint.r * red, tint.g * base.a, tint.b * blue) * glow;
    haylen_output(vec4(shifted, alpha));
}
@end

@program hologram haylen_vs hologram_fs
