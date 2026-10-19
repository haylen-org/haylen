// Burns a sprite away from noise as the progress grows, with glowing embers along the edge and charred color behind them.
@include haylen/material.glsl
@include common.glsl

@fs burnAway_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform burnAway_params {
    vec4 ember_color;
    float progress;
    float edge_width;
};

void main() {
    vec4 base = haylen_base(uv);
    float noise = library_fbm(library_local(uv) * 6.0);
    float threshold = progress * 1.2 - 0.1;
    float edge = 1.0 - smoothstep(threshold, threshold + edge_width, noise);
    float charred = 1.0 - smoothstep(threshold + edge_width, threshold + edge_width * 3.0, noise);
    vec3 shade = mix(mix(base.rgb, base.rgb * 0.2, charred * 0.8), ember_color.rgb * 1.5, edge);
    haylen_output(vec4(shade, base.a * step(threshold, noise)));
}
@end

@program burnAway haylen_vs burnAway_fs
