@include haylen/output.glsl

@vs metaball_vs
layout(binding=0) uniform metaball_vs_params {
    mat4 view_projection;
    // Area: the world rectangle the surface can cover, xy its top-left corner and zw its size.
    vec4 area;
    // Field: x is 1 when the field image starts at its bottom row.
    vec4 field;
};

in vec2 corner;
out vec2 field_uv;

void main() {
    vec4 position = view_projection * vec4(area.xy + corner * area.zw, 0.0, 1.0);
    gl_Position = position;
    vec2 device = position.xy / position.w;
    field_uv = vec2(device.x * 0.5 + 0.5, field.x > 0.5 ? device.y * 0.5 + 0.5 : 0.5 - device.y * 0.5);
}
@end

@fs metaball_fs
layout(binding=1) uniform metaball_fs_params {
    vec4 fill;
    vec4 outline_color;
    // Levels: x is the threshold and y the outline width, both in field units.
    vec4 levels;
};
layout(binding=0) uniform texture2D field_texture;
layout(binding=0) uniform sampler field_sampler;

in vec2 field_uv;

@include_block haylen_output

// The field holds the sum of the soft circles, and the surface is where it reaches the threshold, with the outline in the band just above it.
void main() {
    float value = texture(sampler2D(field_texture, field_sampler), field_uv).r;
    float edge = max(fwidth(value) * 0.75, 0.0001);
    float inside = smoothstep(levels.x - edge, levels.x + edge, value);
    float body = levels.y > 0.0 ? smoothstep(levels.x + levels.y - edge, levels.x + levels.y + edge, value) : 1.0;
    vec4 surface = mix(outline_color, fill, body);
    haylen_output(vec4(surface.rgb, surface.a * inside));
}
@end

@program metaball metaball_vs metaball_fs
