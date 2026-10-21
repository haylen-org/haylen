// Swaps the colors of the parts of a painted sprite: every pixel whose hue is near one of the source colors takes the hue of its target with its own shading, so one sprite serves many teams or skins. Gray, black and white pixels, such as outlines and highlights, keep their colors.
@include haylen/material.glsl

@fs palette_fs
@include_block haylen_fragment
layout(binding=1) uniform palette_params {
    vec4 source_colors[4];
    vec4 target_colors[4];
};

vec3 palette_hsv(vec3 color) {
    vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p = mix(vec4(color.bg, k.wz), vec4(color.gb, k.xy), step(color.b, color.g));
    vec4 q = mix(vec4(p.xyw, color.r), vec4(color.r, p.yzx), step(p.x, color.r));
    float d = q.x - min(q.w, q.y);
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1.0e-10)), d / (q.x + 1.0e-10), q.x);
}

vec3 palette_rgb(vec3 color) {
    vec3 p = abs(fract(color.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
    return color.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), color.y);
}

void main() {
    vec4 base = haylen_base(uv);
    vec3 pixel = palette_hsv(base.rgb);
    vec3 color = base.rgb;
    for (int index = 0; index < 4; index++) {
        vec3 source = palette_hsv(source_colors[index].rgb);
        vec3 target = palette_hsv(target_colors[index].rgb);
        // The hue distance wraps around the circle, and only saturated pixels count as a part.
        float distance = abs(fract(pixel.x - source.x + 0.5) - 0.5);
        float weight = (1.0 - smoothstep(0.035, 0.07, distance)) * smoothstep(0.3, 0.45, pixel.y);
        vec3 swapped = palette_rgb(vec3(target.x, clamp(pixel.y * target.y / max(source.y, 0.01), 0.0, 1.0), clamp(pixel.z * target.z / max(source.z, 0.01), 0.0, 1.0)));
        color = mix(color, swapped, weight);
    }
    haylen_output(vec4(color, base.a));
}
@end

@program palette haylen_vs palette_fs
