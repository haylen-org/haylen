// Burns the draw away where a noise texture falls below the amount, with a glowing edge along the burn.
@include haylen/material.glsl

@fs dissolve_fs
@include_block haylen_fragment
layout(binding=1) uniform dissolve_params {
    vec4 edge_color;
    float amount;
    float edge_width;
    float noise_scale;
};
layout(binding=1) uniform texture2D noise_texture;
layout(binding=1) uniform sampler noise_sampler;

void main() {
    vec4 base = haylen_base(uv);
    float noise = texture(sampler2D(noise_texture, noise_sampler), uv * noise_scale).r;
    float burn = noise - amount;
    if (burn < 0.0) {
        discard;
    }
    float edge = 1.0 - smoothstep(0.0, edge_width, burn);
    haylen_output(vec4(mix(base.rgb, edge_color.rgb, edge * edge_color.a), base.a));
}
@end

@program dissolve haylen_vs dissolve_fs
