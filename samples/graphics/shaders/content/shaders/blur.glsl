// One direction of a Gaussian blur. Two passes, horizontal and vertical, blur the whole image at a fraction of the cost of one square kernel.
@include haylen/material.glsl

@fs blur_fs
@include_block haylen_fragment
layout(binding=1) uniform blur_params {
    vec2 direction;
    float radius;
};

void main() {
    vec2 step = direction * radius;
    vec4 sum = haylen_texture(uv) * 0.227027;
    sum += (haylen_texture(uv + step * 1.384615) + haylen_texture(uv - step * 1.384615)) * 0.316216;
    sum += (haylen_texture(uv + step * 3.230769) + haylen_texture(uv - step * 3.230769)) * 0.070270;
    haylen_output(sum);
}
@end

@program blur haylen_vs blur_fs
