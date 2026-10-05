// A post-processing pass that looks like an old tube screen: curved glass, scanlines, a color split and dark corners.
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
    if (point.x < 0.0 || point.x > 1.0 || point.y < 0.0 || point.y > 1.0) {
        haylen_output(vec4(0.0, 0.0, 0.0, 1.0));
        return;
    }
    vec3 image = vec3(haylen_texture(point + vec2(split, 0.0)).r, haylen_texture(point).g, haylen_texture(point - vec2(split, 0.0)).b);
    float line = 1.0 - scanlines * (0.5 + 0.5 * sin(point.y * 900.0 + time * 4.0));
    float corners = smoothstep(1.0, 0.7, length(centered * vec2(0.9, 1.0)));
    haylen_output(vec4(image * line * (0.6 + 0.4 * corners), 1.0));
}
@end

@program crt haylen_vs crt_fs
