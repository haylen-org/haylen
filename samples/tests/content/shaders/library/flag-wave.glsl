// Waves an image like a flag on a pole at its left side, with the folds shaded.
@include haylen/material.glsl
@include common.glsl

@fs flagWave_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform flagWave_params {
    float amplitude;
    float frequency;
    float speed;
    float time;
};

void main() {
    vec2 local = library_local(uv);
    float phase = local.x * frequency - time * speed;
    vec2 moved = vec2(local.x, local.y + sin(phase) * amplitude * local.x);
    vec4 base = haylen_base(haylen_inside(library_texture_point(moved)));
    float inside = step(0.0, moved.y) * step(moved.y, 1.0);
    haylen_output(vec4(base.rgb * (0.9 + 0.1 * cos(phase)), base.a * inside));
}
@end

@program flagWave haylen_vs flagWave_fs
