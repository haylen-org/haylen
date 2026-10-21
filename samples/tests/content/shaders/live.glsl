// The shader of the hot reload test. Change the stripes, the colors or the speed below, save, and a desktop dev run shows the result on the next frame.
@include haylen/material.glsl

@fs live_fs
@include_block haylen_fragment
layout(binding=1) uniform live_params {
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    float stripes = smoothstep(0.35, 0.65, 0.5 + 0.5 * sin((uv.x + uv.y) * 24.0 - time * 3.0));
    vec3 first = vec3(1.0, 0.79, 0.24);
    vec3 second = vec3(0.18, 0.53, 0.96);
    vec3 light = mix(first, second, stripes);
    haylen_output(vec4(base.rgb * mix(vec3(1.0), light * 1.3, 0.75), base.a));
}
@end

@program live haylen_vs live_fs
