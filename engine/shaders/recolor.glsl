// Recolors a white or greyscale sprite by the parts a mask with the layout of its texture marks in red, green, blue and yellow, each tinted by its own color of the instance, which keeps the shading of the sprite.
@include haylen/material.glsl

@vs recolor_vs
@include_block haylen_vertex
in vec4 instance_red;
in vec4 instance_green;
in vec4 instance_blue;
in vec4 instance_yellow;
out vec4 recolor_red;
out vec4 recolor_green;
out vec4 recolor_blue;
out vec4 recolor_yellow;

void main() {
    haylen_vertex_main();
    recolor_red = instance_red;
    recolor_green = instance_green;
    recolor_blue = instance_blue;
    recolor_yellow = instance_yellow;
}
@end

@fs recolor_fs
@include_block haylen_fragment
layout(binding=1) uniform texture2D part_mask;
in vec4 recolor_red;
in vec4 recolor_green;
in vec4 recolor_blue;
in vec4 recolor_yellow;

// A part color multiplies the base where its alpha is full and leaves it alone where its alpha is 0.
vec3 recolor_part(vec4 tint) {
    return mix(vec3(1.0), tint.rgb, tint.a);
}

// Yellow is red and green together, so it counts once and leaves the rest of each to its own part, and the alpha of the mask fades the parts out at their soft edges.
void main() {
    vec2 point = haylen_inside(uv);
    vec4 mask = texture(sampler2D(part_mask, sprite_sampler), point);
    float yellow = min(mask.r, mask.g);
    vec4 weights = vec4(mask.r - yellow, mask.g - yellow, mask.b, yellow) * mask.a;
    vec3 tint = recolor_part(recolor_red) * weights.x + recolor_part(recolor_green) * weights.y + recolor_part(recolor_blue) * weights.z + recolor_part(recolor_yellow) * weights.w + vec3(max(0.0, 1.0 - dot(weights, vec4(1.0))));
    vec4 base = haylen_texture(point);
    vec4 shaded = vec4(base.rgb * tint, base.a) * color;
    shaded.rgb = mix(shaded.rgb, haylen_flash.rgb, haylen_flash.a);
    haylen_output(shaded);
}
@end

@program recolor recolor_vs recolor_fs
