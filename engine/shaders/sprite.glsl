@include haylen/material.glsl

@fs sprite_fs
@include_block haylen_fragment
#ifdef HAYLEN_LIT
layout(binding=1) uniform texture2D normal_texture;
#endif

void main() {
    vec4 base = haylen_sprite(uv);
#ifdef HAYLEN_LIT
    // A normal-mapped draw has a shininess, and the alpha of its normal map scales its specular strength.
    if (haylen_info.z > 0.0) {
        vec4 mapped = texture(sampler2D(normal_texture, sprite_sampler), haylen_inside(uv));
        haylen_output_surface(base, haylen_world_normal(mapped.xyz * 2.0 - 1.0), haylen_surface.x * mapped.a, haylen_info.z);
        return;
    }
#endif
    haylen_output(base);
}
@end

@program sprite haylen_vs sprite_fs
