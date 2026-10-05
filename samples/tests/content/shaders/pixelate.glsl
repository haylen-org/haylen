// Samples the image in blocks, so it breaks into big pixels. The block count sets how many blocks cover the image on each axis.
@include haylen/material.glsl

@fs pixelate_fs
@include_block haylen_fragment
layout(binding=1) uniform pixelate_params {
    vec2 blocks;
};

void main() {
    vec2 point = (floor(uv * blocks) + 0.5) / blocks;
    haylen_output(haylen_base(point));
}
@end

@program pixelate haylen_vs pixelate_fs
