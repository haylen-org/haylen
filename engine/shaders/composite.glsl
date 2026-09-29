@vs composite_vs
out vec2 uv;

void main() {
    // A single triangle covers the viewport with texture coordinates whose origin is the top-left corner.
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(corner.x, 1.0 - corner.y);
}
@end

@fs composite_fs
layout(binding=0) uniform composite_fs_params {
    vec4 tint;
    vec4 fade;
    vec4 grading;
    vec4 vignette;
    vec4 flags;
};
layout(binding=0) uniform texture2D scene_texture;
layout(binding=1) uniform texture2D light_texture;
layout(binding=2) uniform texture2D emission_texture;
layout(binding=0) uniform sampler composite_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    // Flags: x enables the light map and the emission, and y flips rows for backends whose render targets start at the bottom.
    vec2 sample_uv = flags.y > 0.5 ? vec2(uv.x, 1.0 - uv.y) : uv;
    // The scene holds colors premultiplied by its coverage, which stays below 1 where a transparent window lets the desktop through.
    vec4 scene = texture(sampler2D(scene_texture, composite_sampler), sample_uv);
    vec3 color = scene.rgb;
    float alpha = scene.a;

    // Emitted light also covers what lies behind a transparent window, the way an additive draw does.
    if (flags.x > 0.5) {
        vec3 emission = texture(sampler2D(emission_texture, composite_sampler), sample_uv).rgb;
        color *= texture(sampler2D(light_texture, composite_sampler), sample_uv).rgb;
        color += emission;
        alpha = max(alpha, max(emission.r, max(emission.g, emission.b)));
    }

    // Grading: x is saturation, y is brightness and z is contrast, which pivots on the middle gray of the coverage.
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, grading.x);
    color = (color - 0.5 * alpha) * grading.z + 0.5 * alpha;
    color *= grading.y;
    color *= tint.rgb;

    // Vignette: x is strength, y is inner radius and z is softness.
    float distance_to_center = length(uv - 0.5) * 1.41421356;
    float shade = smoothstep(vignette.y, vignette.y + vignette.z, distance_to_center);
    color *= 1.0 - shade * vignette.x;

    // The fade covers the canvas with an opaque color.
    color = mix(color, fade.rgb, fade.a);
    alpha = min(mix(alpha, 1.0, fade.a), 1.0);
    frag_color = vec4(clamp(color, vec3(0.0), vec3(alpha)), alpha);
}
@end

@program composite composite_vs composite_fs
