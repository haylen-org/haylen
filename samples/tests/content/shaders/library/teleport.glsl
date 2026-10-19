// Builds a sprite from the bottom up behind a bright scan line with digital noise, as it teleports in, and takes it apart as the progress falls.
@include haylen/material.glsl
@include common.glsl

@fs teleport_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform teleport_params {
    vec4 beam_color;
    float progress;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    float line = 1.0 - progress * 1.1;
    float shown = step(line, local.y);
    float scan = exp(-abs(local.y - line) * 60.0);
    float blocks = step(0.6, library_hash(floor(local * vec2(24.0, 48.0)) + floor(time * 15.0))) * smoothstep(0.12, 0.0, abs(local.y - line));
    vec3 shade = mix(base.rgb, beam_color.rgb, scan) + beam_color.rgb * blocks * 0.6;
    haylen_output(vec4(shade, base.a * max(shown, max(scan, blocks * 0.8))));
}
@end

@program teleport haylen_vs teleport_fs
