// The shader of the hot reload test. Change the stripes, the colors or the speed below, save, and a desktop dev run shows the result on the next frame.
@include haylen/material.glsl

@fs live_fs
@include_block haylen_fragment
layout(binding=1) uniform live_params {
    float time;
};

void main() {
    vec4 base = haylen_base(uv);
    float stripes = 0.5 + 0.5 * sin((uv.x + uv.y) * 40.0 - time * 3.0);
    vec3 first = vec3(0.95, 0.35, 0.55);
    vec3 second = vec3(0.25, 0.75, 1.0);
    haylen_output(vec4(base.rgb * mix(first, second, stripes), base.a));
}
@end

@program live haylen_vs live_fs
