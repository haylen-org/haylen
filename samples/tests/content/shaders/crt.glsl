// A post-processing pass that looks like an old tube screen: curved glass with rounded corners whose edge fades out over a pixel, scanlines, a color split and dark corners.
@include haylen/material.glsl

@fs crt_fs
@include_block haylen_fragment
layout(binding=1) uniform crt_params {
    float curvature;
    float scanlines;
    float split;
    float time;
};

void main() {
    vec2 centered = uv * 2.0 - 1.0;
    centered *= 1.0 + curvature * dot(centered.yx, centered.yx);
    vec2 point = centered * 0.5 + 0.5;
    // The glass is a rounded rectangle, and its edge fades over the width of a pixel of the screen.
    vec2 reach = abs(centered) - vec2(1.0 - 0.08);
    float outside = length(max(reach, 0.0)) + min(max(reach.x, reach.y), 0.0) - 0.08;
    float pixel = max(fwidth(centered.x), fwidth(centered.y));
    float glass = 1.0 - smoothstep(-pixel, pixel, outside);
    vec2 inside = clamp(point, 0.0, 1.0);
    vec3 image = vec3(haylen_texture(inside + vec2(split, 0.0)).r, haylen_texture(inside).g, haylen_texture(inside - vec2(split, 0.0)).b);
    float line = 1.0 - scanlines * (0.5 + 0.5 * sin(inside.y * 900.0 + time * 4.0));
    float corners = smoothstep(1.0, 0.7, length(centered * vec2(0.9, 1.0)));
    haylen_output(vec4(image * line * (0.6 + 0.4 * corners), glass));
}
@end

@program crt haylen_vs crt_fs
