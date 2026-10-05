// Swaps the colors of pixel art: every pixel takes the target color of the nearest source color, so one sprite serves many teams or skins.
@include haylen/material.glsl

@fs palette_fs
@include_block haylen_fragment
layout(binding=1) uniform palette_params {
    vec4 source_colors[6];
    vec4 target_colors[6];
};

void main() {
    vec4 base = haylen_base(uv);
    int nearest = 0;
    float best = 1000.0;
    for (int index = 0; index < 6; index++) {
        vec3 difference = base.rgb - source_colors[index].rgb;
        float distance = dot(difference, difference);
        if (distance < best) {
            best = distance;
            nearest = index;
        }
    }
    haylen_output(vec4(target_colors[nearest].rgb, base.a));
}
@end

@program palette haylen_vs palette_fs
