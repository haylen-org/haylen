// Text in a rainbow that flows across the screen, with a shine that sweeps over the letters. It keeps the glyph shape and outline of the text program.
@include haylen/material.glsl

@fs rainbow_fs
@include_block haylen_fragment
layout(binding=1) uniform rainbow_params {
    float time;
    float spread;
};

vec3 hue(float value) {
    return clamp(abs(fract(value + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

void main() {
    vec4 base = haylen_base(uv);
    float shine = smoothstep(0.92, 1.0, sin(gl_FragCoord.x * 0.01 - time * 3.0));
    vec3 rainbow = mix(hue(gl_FragCoord.x * spread - time * 0.3), vec3(1.0), shine);
    // The rainbow scales with the brightness of the glyph, so a white fill takes it and a dark outline stays dark.
    haylen_output(vec4(rainbow * dot(base.rgb, vec3(0.3333)), base.a));
}
@end

@program rainbow haylen_vs rainbow_fs
