// Color grading that tints the shadows and the highlights apart, like the teal and orange of films, with its own contrast.
@include haylen/material.glsl

@fs grading_fs
@include_block haylen_fragment
layout(binding=1) uniform grading_params {
    vec4 shadows;
    vec4 highlights;
    float strength;
    float contrast;
};

void main() {
    vec4 image = haylen_texture(uv);
    float luminance = dot(image.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 toned = mix(shadows.rgb, highlights.rgb, smoothstep(0.1, 0.9, luminance));
    vec3 graded = mix(image.rgb, image.rgb * toned * 2.0, strength);
    graded = (graded - 0.5) * contrast + 0.5;
    haylen_output(vec4(clamp(graded, 0.0, 1.0), image.a));
}
@end

@program grading haylen_vs grading_fs
