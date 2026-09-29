@include haylen/material.glsl

@fs mesh_fs
@include_block haylen_fragment

void main() {
    haylen_output(haylen_sprite(uv));
}
@end

@program mesh haylen_vs mesh_fs
