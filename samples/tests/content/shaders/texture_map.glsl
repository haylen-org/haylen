// Recolors the draw through a gradient texture by its brightness and lays a scrolling pattern texture over it, two textures bound as uniforms next to the texture of the draw.
@include haylen/material.glsl

@fs texture_map_fs
@include_block haylen_fragment
layout(binding=1) uniform texture_map_params {
    vec2 scroll;
    float pattern_strength;
    float pattern_scale;
};
layout(binding=1) uniform texture2D ramp_texture;
layout(binding=1) uniform sampler ramp_sampler;
layout(binding=2) uniform texture2D pattern_texture;
layout(binding=2) uniform sampler pattern_sampler;

void main() {
    vec4 base = haylen_base(uv);
    float brightness = dot(base.rgb, vec3(0.2126, 0.7152, 0.0722));
    vec3 mapped = texture(sampler2D(ramp_texture, ramp_sampler), vec2(brightness, 0.5)).rgb;
    float pattern = texture(sampler2D(pattern_texture, pattern_sampler), uv * pattern_scale + scroll).r;
    haylen_output(vec4(mapped * mix(1.0, pattern, pattern_strength), base.a));
}
@end

@program texture_map haylen_vs texture_map_fs
