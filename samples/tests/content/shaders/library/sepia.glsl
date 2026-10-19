// Turns the colors into the browns of an old photograph, by the amount.
@include haylen/material.glsl
@include common.glsl

@fs sepia_fs
@include_block haylen_fragment
@include_block library_common
layout(binding=1) uniform sepia_params {
    float amount;
};

void main() {
    vec4 base = haylen_base(uv);
    vec3 sepia = vec3(dot(base.rgb, vec3(0.393, 0.769, 0.189)), dot(base.rgb, vec3(0.349, 0.686, 0.168)), dot(base.rgb, vec3(0.272, 0.534, 0.131)));
    haylen_output(vec4(mix(base.rgb, min(sepia, vec3(1.0)), amount), base.a));
}
@end

@program sepia haylen_vs sepia_fs
