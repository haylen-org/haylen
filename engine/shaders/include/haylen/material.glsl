// The Haylen shader library for 2D draws. A material is a fragment shader that includes this file, declares its program with the haylen_vs vertex stage and writes its color with haylen_output. The engine compiles every material for sprites, text and meshes, and for lit canvases, through the HAYLEN_TEXT, HAYLEN_MESH and HAYLEN_LIT defines.
@include haylen/output.glsl

@vs haylen_vs
layout(binding=0) uniform haylen_vs_params {
    mat4 view_projection;
    // 1 when the blend mode of the draw expects colors premultiplied by their alpha, as multiply and screen do.
    float haylen_premultiply;
};

#ifdef HAYLEN_MESH
in vec2 position;
in vec2 texcoord;
in vec4 color0;
#else
in vec2 corner;
in vec2 instance_position;
in vec2 instance_size;
in vec4 instance_uv;
in vec4 instance_color;
in vec4 instance_flash;
in float instance_rotation;
in vec4 instance_parameters;
in vec2 instance_pivot;
#endif

out vec2 uv;
out vec4 color;
out vec4 haylen_flash;
out vec3 haylen_text_style;
out vec2 haylen_transform;
out float haylen_output_premultiply;

// Decodes a two's complement byte of the instance parameters.
float haylen_signed_byte(float value) {
    float step = floor(value * 255.0 + 0.5);
    return step < 128.0 ? step : step - 256.0;
}

void main() {
    haylen_output_premultiply = haylen_premultiply;
#ifdef HAYLEN_MESH
    gl_Position = view_projection * vec4(position, 0.0, 1.0);
    uv = texcoord;
    color = color0;
    haylen_flash = vec4(0.0);
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(0.0);
#else
    // The skew leans the quad around its pivot, which glyphs place on their baseline, before the rotation turns it.
    float angle = instance_rotation;
    vec2 local = (corner - instance_pivot) * instance_size;
    local.x -= haylen_signed_byte(instance_parameters.z) / 127.0 * local.y;
    float sine = sin(angle);
    float cosine = cos(angle);
    vec2 world = instance_position + vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
    gl_Position = view_projection * vec4(world, 0.0, 1.0);

    // Text reads the outline, the weight and the softness from its parameters, and sprites read flip flags that follow Tiled: bit 0 flips horizontally, bit 1 vertically and bit 2 swaps the axes. Tiled swaps the image first and flips the result, so the lookup flips first and swaps last.
#ifdef HAYLEN_TEXT
    float flags = 0.0;
    haylen_text_style = vec3(instance_parameters.x * 0.5, haylen_signed_byte(instance_parameters.y) / 256.0, instance_parameters.w * 0.5);
#else
    float flags = floor(instance_parameters.x * 255.0 + 0.5);
    haylen_text_style = vec3(0.0);
#endif
    vec2 texel = corner;
    if (mod(flags, 2.0) >= 1.0) {
        texel.x = 1.0 - texel.x;
    }
    if (mod(floor(flags / 2.0), 2.0) >= 1.0) {
        texel.y = 1.0 - texel.y;
    }
    if (mod(floor(flags / 4.0), 2.0) >= 1.0) {
        texel = texel.yx;
    }

    uv = mix(instance_uv.xy, instance_uv.zw, texel);
    color = instance_color;
    haylen_flash = instance_flash;
    haylen_transform = vec2(angle, flags);
#endif
}
@end

@block haylen_fragment
layout(binding=0) uniform texture2D sprite_texture;
layout(binding=0) uniform sampler sprite_sampler;

in vec2 uv;
in vec4 color;
in vec4 haylen_flash;
in vec3 haylen_text_style;
in vec2 haylen_transform;

@include_block haylen_output

vec4 haylen_texture(vec2 point) {
    return texture(sampler2D(sprite_texture, sprite_sampler), point);
}

// The texture color multiplied by the draw color and mixed toward the flash color by its alpha.
vec4 haylen_sprite(vec2 point) {
    vec4 shaded = haylen_texture(point) * color;
    shaded.rgb = mix(shaded.rgb, haylen_flash.rgb, haylen_flash.a);
    return shaded;
}

// A glyph of a text atlas, which stores a signed distance field whose edge is 0.5, filled with the draw color and outlined with the flash color. The weight moves the edge outward, which makes the glyph bolder, and the softness widens the edge into a blur for soft shadows and glows. The fill covers the outline in premultiplied colors, so each coverage counts once, and the result returns to straight colors.
vec4 haylen_text(vec2 point) {
    float distance = haylen_texture(point).r;
    float edge = 0.5 - haylen_text_style.y;
    float smoothing = max(fwidth(distance) * 0.7, 0.0001) + haylen_text_style.z;
    float fill = smoothstep(edge - smoothing, edge + smoothing, distance);
    float border = smoothstep(edge - haylen_text_style.x - smoothing, edge - haylen_text_style.x + smoothing, distance);
    vec4 inner = vec4(color.rgb * color.a, color.a) * fill;
    vec4 outer = vec4(haylen_flash.rgb * haylen_flash.a, haylen_flash.a) * border;
    vec4 glyph = inner + outer * (1.0 - inner.a);
    return vec4(glyph.rgb / max(glyph.a, 0.0001), glyph.a);
}

// The color the draw has without a material: the sprite, or the glyph when the material draws text.
vec4 haylen_base(vec2 point) {
#ifdef HAYLEN_TEXT
    return haylen_text(point);
#else
    return haylen_sprite(point);
#endif
}

// Turns a tangent-space normal with y pointing up the image, such as a normal map texel mapped to -1 to 1, into the world, through the flips and the rotation of the sprite.
vec2 haylen_world_normal(vec3 tangent) {
    float flags = haylen_transform.y;
    vec2 normal = vec2(tangent.x, -tangent.y);
    if (mod(floor(flags / 4.0), 2.0) >= 1.0) {
        normal = normal.yx;
    }
    if (mod(flags, 2.0) >= 1.0) {
        normal.x = -normal.x;
    }
    if (mod(floor(flags / 2.0), 2.0) >= 1.0) {
        normal.y = -normal.y;
    }
    float sine = sin(haylen_transform.x);
    float cosine = cos(haylen_transform.x);
    return vec2(normal.x * cosine - normal.y * sine, normal.x * sine + normal.y * cosine);
}
@end
