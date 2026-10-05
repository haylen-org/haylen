// The Haylen shader library for 2D draws. A material is a fragment shader that includes this file, declares its program with the `haylen_vs` vertex stage and writes its color with `haylen_output`. The engine compiles every material for sprites, text, meshes and shapes, and for lit canvases, through the `HAYLEN_TEXT`, `HAYLEN_MESH`, `HAYLEN_SHAPE` and `HAYLEN_LIT` defines.
@include haylen/output.glsl

// The vertex stage of every 2D program, which programs with attributes of their own include and call from their `main`.
@block haylen_vertex
layout(binding=0) uniform haylen_vs_params {
    mat4 view_projection;
    // 1 when the blend mode of the draw expects colors premultiplied by their alpha, as `multiply` and `screen` do.
    float haylen_premultiply;
};

#ifdef HAYLEN_MESH
in vec2 position;
in vec2 texcoord;
in vec4 color0;
#elif defined(HAYLEN_SHAPE)
in vec2 corner;
in vec2 shape_center;
in vec2 shape_half_size;
in float shape_rotation;
in vec2 shape_reach;
in vec4 shape_color;
in vec4 shape_border_color;
in vec4 shape_form;
in vec4 shape_radii;
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
out vec4 haylen_source;
#ifdef HAYLEN_SHAPE
out vec4 haylen_shape_frame;
out vec4 haylen_shape_sector_point;
out vec4 haylen_shape_form;
out vec4 haylen_shape_radii;
out vec4 haylen_shape_border;
#endif
out float haylen_output_premultiply;

// Decodes a two's complement byte of the instance parameters.
float haylen_signed_byte(float value) {
    float step = floor(value * 255.0 + 0.5);
    return step < 128.0 ? step : step - 256.0;
}

// Places the corner of the quad of an instance, or the vertex of a mesh, and hands the fragment stage what it reads.
void haylen_vertex_main() {
    haylen_output_premultiply = haylen_premultiply;
#ifdef HAYLEN_MESH
    gl_Position = view_projection * vec4(position, 0.0, 1.0);
    uv = texcoord;
    color = color0;
    haylen_flash = vec4(0.0);
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(0.0);
    haylen_source = vec4(0.0);
#elif defined(HAYLEN_SHAPE)
    // The quad reaches past the edge by the margin, where the edge fades out, and turns with the shape around its center, while the texture coordinates go from 0 to 1 across the rectangle of the shape. The fragment stage measures from the center in the frame of the shape, where the border and the radii of the corners take their shares of the shorter half side, and from the same point turned so the sector lies around the x axis.
    vec2 local = (corner * 2.0 - 1.0) * (shape_half_size + shape_reach.y);
    float sine = sin(shape_rotation);
    float cosine = cos(shape_rotation);
    vec2 world = shape_center + vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
    gl_Position = view_projection * vec4(world, 0.0, 1.0);
    uv = local / shape_half_size * 0.5 + 0.5;
    color = shape_color;
    haylen_flash = vec4(0.0);
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(shape_rotation, 0.0);
    haylen_source = vec4(0.0, 0.0, 1.0, 1.0);
    float shorter = min(shape_half_size.x, shape_half_size.y);
    float middle = (shape_form.z + shape_form.y * 0.5) * 6.28318531;
    haylen_shape_frame = vec4(local, shape_half_size);
    haylen_shape_sector_point = vec4(local.x * cos(middle) + local.y * sin(middle), local.y * cos(middle) - local.x * sin(middle), shape_form.y, 0.0);
    haylen_shape_form = vec4(shape_form.x * shorter, shape_reach.x, shorter, 0.0);
    haylen_shape_radii = shape_radii * shorter;
    haylen_shape_border = shape_border_color;
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
    haylen_source = vec4(min(instance_uv.xy, instance_uv.zw), max(instance_uv.xy, instance_uv.zw));
    color = instance_color;
    haylen_flash = instance_flash;
    haylen_transform = vec2(angle, flags);
#endif
}
@end

@vs haylen_vs
@include_block haylen_vertex

void main() {
    haylen_vertex_main();
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
in vec4 haylen_source;

#ifdef HAYLEN_SHAPE
in vec4 haylen_shape_frame;
in vec4 haylen_shape_sector_point;
in vec4 haylen_shape_form;
in vec4 haylen_shape_radii;
in vec4 haylen_shape_border;

// The signed distance from a point in the frame of the shape to the edge of its rounded rectangle, negative inside, with the radius of the corner of the quarter the point lies in.
float haylen_shape_box(vec2 point) {
    vec4 radii = haylen_shape_radii;
    float radius = point.x < 0.0 ? (point.y < 0.0 ? radii.x : radii.w) : (point.y < 0.0 ? radii.y : radii.z);
    vec2 corner = abs(point) - haylen_shape_frame.zw + radius;
    return min(max(corner.x, corner.y), 0.0) + length(max(corner, 0.0)) - radius;
}

// The signed distance to the sides of the sector the sweep keeps, from the point turned so the sector lies around the x axis, where the distance to the nearer side follows from its angle, while a full turn keeps every point.
float haylen_shape_sector() {
    if (haylen_shape_sector_point.z >= 1.0) {
        return -haylen_shape_form.z;
    }
    float half_angle = haylen_shape_sector_point.z * 3.14159265;
    vec2 turned = haylen_shape_sector_point.xy;
    return abs(turned.y) * cos(half_angle) - turned.x * sin(half_angle);
}

// How much of the pixel the shape covers, and how much of it its fill covers inside the border. Each edge fades over one pixel of the screen, or over the softness, so the coverage of a pixel follows how far its center lies inside the edge. The coverage past the far side of a thin shape is taken away again, so shapes thinner than a pixel cover only what they cross.
vec2 haylen_shape_cover() {
    vec2 point = haylen_shape_frame.xy;
    float pixel = 0.5 * (length(dFdx(point)) + length(dFdy(point)));
    float ramp = max(max(pixel, haylen_shape_form.y), 0.000001);
    float box = haylen_shape_box(point);
    float sector = haylen_shape_sector();
    float beyond = clamp(0.5 - (box + 2.0 * haylen_shape_form.z) / ramp, 0.0, 1.0);
    float outer = clamp(0.5 - max(box, sector) / ramp, 0.0, 1.0) - beyond;
    float inner = clamp(0.5 - max(box + haylen_shape_form.x, sector) / ramp, 0.0, 1.0) - beyond;
    return vec2(max(outer, 0.0), max(inner, 0.0));
}

// Paints the fill inside the border and the border color in the band along the edge, as one straight color for the whole shape, which `haylen_output` fades by the coverage of the shape.
vec4 haylen_shape_paint(vec4 fill) {
    vec2 cover = haylen_shape_cover();
    vec4 border = haylen_shape_border;
    vec4 painted = vec4(fill.rgb * fill.a, fill.a) * cover.y + vec4(border.rgb * border.a, border.a) * (cover.x - cover.y);
    return vec4(painted.rgb / max(painted.a, 0.0001), painted.a / max(cover.x, 0.0001));
}
#endif

@include_block haylen_output

vec4 haylen_texture(vec2 point) {
    return texture(sampler2D(sprite_texture, sprite_sampler), point);
}

// Moves a point inside the texels of the source rectangle of the draw, so filtering never reads the texels around it, such as the neighbours of a region of an atlas or the borders of a stretched nine-slice center. Meshes and shapes keep their texture coordinates.
vec2 haylen_inside(vec2 point) {
#if defined(HAYLEN_MESH) || defined(HAYLEN_SHAPE)
    return point;
#else
    vec2 margin = 0.5 / vec2(textureSize(sampler2D(sprite_texture, sprite_sampler), 0));
    vec2 middle = (haylen_source.xy + haylen_source.zw) * 0.5;
    return clamp(point, min(haylen_source.xy + margin, middle), max(haylen_source.zw - margin, middle));
#endif
}

// The texture color inside the source of the draw multiplied by the draw color and mixed toward the flash color by its alpha, which a shape paints with its border.
vec4 haylen_sprite(vec2 point) {
    vec4 shaded = haylen_texture(haylen_inside(point)) * color;
    shaded.rgb = mix(shaded.rgb, haylen_flash.rgb, haylen_flash.a);
#ifdef HAYLEN_SHAPE
    shaded = haylen_shape_paint(shaded);
#endif
    return shaded;
}

// A glyph of a text atlas, which stores a signed distance field whose edge is 0.5, filled with the draw color and outlined with the flash color. The weight moves the edge outward, which makes the glyph bolder, and the softness widens the edge into a blur for soft shadows and glows. With the smoothing of the edge on screen they shrink together to stop a sixteenth of the field short of 0, which the border of the quad of the glyph reaches, so a glyph never shows its quad at any size. The fill covers the outline in premultiplied colors, so each coverage counts once, and the result returns to straight colors.
vec4 haylen_text(vec2 point) {
    float distance = haylen_texture(haylen_inside(point)).r;
    float smoothing = max(fwidth(distance) * 0.7, 0.0001);
    float reach = max(haylen_text_style.y, 0.0) + haylen_text_style.x + haylen_text_style.z;
    float room = max(0.4375 - smoothing, 0.0);
    float fit = reach > room ? room / reach : 1.0;
    float edge = 0.5 - haylen_text_style.y * (haylen_text_style.y > 0.0 ? fit : 1.0);
    float outline = haylen_text_style.x * fit;
    float spread = smoothing + haylen_text_style.z * fit;
    float fill = smoothstep(edge - spread, edge + spread, distance);
    float border = smoothstep(edge - outline - spread, edge - outline + spread, distance);
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
