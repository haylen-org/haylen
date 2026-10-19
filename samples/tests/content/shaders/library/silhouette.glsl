// Fills a sprite with one color, for a character seen behind a wall or a locked item.
@include haylen/material.glsl
@include common.glsl

@fs silhouette_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform silhouette_params {
    vec4 silhouette_color;
};

void main() {
    vec4 base = haylen_base(uv);
    haylen_output(vec4(silhouette_color.rgb, base.a * silhouette_color.a));
}
@end

@program silhouette haylen_vs silhouette_fs
