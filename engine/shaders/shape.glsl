@include haylen/material.glsl

@fs shape_fs
@include_block haylen_fragment

void main() {
    haylen_output(haylen_draw_sprite());
}
@end

@program shape haylen_vs shape_fs
