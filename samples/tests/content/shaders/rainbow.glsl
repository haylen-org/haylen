// A soft rainbow that flows across the screen, with a narrow shine that sweeps over it on a diagonal. It keeps the shape and outline of the draw, so it serves text and sprites alike.
@include haylen/material.glsl

@fs rainbow_fs
@include_block haylen_fragment
layout(binding=1) uniform rainbow_params {
    float time;
    float spread;
};

// Colors of candy that blend in a loop, brighter and softer than the pure hues of a color wheel.
vec3 rainbow_color(float value) {
    vec3 a = vec3(0.72, 0.62, 0.72);
    vec3 b = vec3(0.28, 0.32, 0.26);
    return a + b * cos(6.28318 * (value + vec3(0.0, 0.33, 0.67)));
}

void main() {
    vec4 base = haylen_base(uv);
    float sweep = (gl_FragCoord.x + gl_FragCoord.y * 0.6) * 0.004 - time * 0.6;
    float shine = pow(max(0.0, sin(sweep)), 60.0) * 0.7;
    vec3 rainbow = rainbow_color(gl_FragCoord.x * spread - time * 0.25);
    // The rainbow scales with the brightness of the draw, lifted in the middle tones, so a white fill takes it whole, a painted sprite takes it in its own shading and a dark outline stays dark, and the shine adds light on top.
    float brightness = dot(base.rgb, vec3(0.3333));
    float lift = brightness * (1.8 - 0.8 * brightness);
    haylen_output(vec4(rainbow * lift + shine * lift, base.a));
}
@end

@program rainbow haylen_vs rainbow_fs
