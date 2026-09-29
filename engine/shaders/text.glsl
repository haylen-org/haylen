@include haylen/material.glsl

@fs text_fs
@include_block haylen_fragment

void main() {
    haylen_output(haylen_text(uv));
}
@end

@program text haylen_vs text_fs
