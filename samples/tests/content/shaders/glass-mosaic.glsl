// A glass effect of the test project: what lies behind the glass shows as a mosaic of square cells of the canvas, like privacy glass, with thin light lines between the cells.
@include haylen/material.glsl

@fs glass_mosaic_fs
@include_block haylen_fragment
layout(binding=1) uniform glass_mosaic_params {
    float cell;
    float lines;
};

void main() {
#ifdef HAYLEN_SHAPE
    vec2 point = haylen_output_point;
    vec2 middle = (floor(point / cell) + 0.5) * cell;
    vec4 behind = haylen_backdrop(middle - point);
    vec2 inside = abs(fract(point / cell) - 0.5) * 2.0;
    float line = smoothstep(1.0 - lines, 1.0, max(inside.x, inside.y));
    vec3 shaded = mix(behind.rgb / max(behind.a, 0.0001), vec3(1.0), line * 0.35);
    haylen_output(vec4(shaded, 1.0) * color);
#else
    haylen_output(haylen_base(uv));
#endif
}
@end

@program glass_mosaic haylen_vs glass_mosaic_fs
