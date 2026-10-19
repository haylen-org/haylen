// Shows an image through night vision goggles: bright green, grainy, with scan lines and a dark rim.
@include haylen/material.glsl
@include common.glsl

@fs nightVision_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform nightVision_params {
    float grain;
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    vec2 local = library_local(uv);
    float noise = library_hash(local * 500.0 + time * 60.0) * grain;
    float scan = 0.9 + 0.1 * sin(local.y * 400.0 + time * 20.0);
    float rim = smoothstep(0.85, 0.3, length(local - 0.5));
    haylen_output(vec4(vec3(0.15, 1.0, 0.25) * (library_luminance(base.rgb) * 1.6 + noise) * scan * rim, base.a));
}
@end

@program nightVision haylen_vs nightVision_fs
