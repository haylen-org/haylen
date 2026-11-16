// A material of glass that shows what lies behind it 24 units to the left, with its red and green swapped, the way an effect of an app reads the image behind a glass.
@include haylen/material.glsl

@fs shift_fs
@include_block haylen_fragment

void main() {
#ifdef HAYLEN_SHAPE
    vec4 behind = haylen_backdrop(vec2(-24.0, 0.0));
    haylen_output(vec4(behind.g, behind.r, behind.b, 1.0) * color);
#else
    haylen_output(haylen_base(uv));
#endif
}
@end

@program shift haylen_vs shift_fs
