// The Haylen shader library for 2D draws. A material is a fragment shader that includes this file, declares its program with the `haylen_vs` vertex stage and writes its color with `haylen_output`. The engine compiles every material for sprites, sprite batches with several textures or sprite data, text, meshes and shapes, and for lit canvases, through the `HAYLEN_BATCH`, `HAYLEN_TEXT`, `HAYLEN_MESH`, `HAYLEN_SHAPE` and `HAYLEN_LIT` defines. Glass draws use the shape program of their material.
@include haylen/output.glsl

// The vertex stage of every 2D program, which programs with attributes of their own include and call from their `main`.
@block haylen_vertex
layout(binding=0) uniform haylen_vs_params {
    mat4 view_projection;
    // 1 when the blend mode of the draw expects colors premultiplied by their alpha, as `multiply` and `screen` do.
    float haylen_premultiply;
    // The size of the viewport of the pass in pixels, which turns lengths of the canvas into pixels.
    vec2 haylen_viewport;
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
#ifdef HAYLEN_BATCH
// A sprite of a batch with several textures or sprite data reads a second record with its four numbers and the texture it draws from.
in vec4 instance_data;
in float instance_texture;
#endif
#endif

// The values of an instance hold across its quad, so they reach the fragment stage flat, without the work of interpolating them, while the vertices of a mesh each have their own color.
out vec2 uv;
#ifdef HAYLEN_MESH
out vec4 color;
#else
flat out vec4 color;
#endif
flat out vec4 haylen_flash;
flat out vec3 haylen_text_style;
flat out vec2 haylen_transform;
flat out vec4 haylen_source;
flat out vec4 haylen_data;
#ifdef HAYLEN_BATCH
flat out float haylen_texture_index;
#endif
#ifdef HAYLEN_SHAPE
out vec4 haylen_shape_frame;
out vec4 haylen_shape_sector_point;
flat out vec4 haylen_shape_form;
flat out vec4 haylen_shape_radii;
flat out vec4 haylen_shape_border;
#endif
flat out float haylen_output_premultiply;
out vec2 haylen_output_point;

// Decodes a two's complement byte of the instance parameters.
float haylen_signed_byte(float value) {
    float step = floor(value * 255.0 + 0.5);
    return step < 128.0 ? step : step - 256.0;
}

#if !defined(HAYLEN_MESH) && !defined(HAYLEN_SHAPE)
// The source rectangle of a sprite in texture coordinates. A source that reaches past its texture spreads them over `2^range` textures, from 0 or centered on 0, with the range in bits 3 to 6 of the flags of the sprite and the centering in bit 7, so the texture repeats or mirrors by its wrap across the quad.
vec4 haylen_instance_source(float flags) {
    float span = exp2(mod(floor(flags / 8.0), 16.0));
    float shift = floor(flags / 128.0) * span * 0.5;
    return instance_uv * span - shift;
}
#endif

// Places the corner of the quad of an instance, or the vertex of a mesh, and hands the fragment stage what it reads.
void haylen_vertex_main() {
    haylen_output_premultiply = haylen_premultiply;
#ifdef HAYLEN_MESH
    gl_Position = view_projection * vec4(position, 0.0, 1.0);
    haylen_output_point = position;
    uv = texcoord;
    color = color0;
    haylen_flash = vec4(0.0);
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(0.0);
    haylen_source = vec4(0.0, 0.0, 1.0, 1.0);
    haylen_data = vec4(0.0);
#elif defined(HAYLEN_SHAPE)
    // The quad reaches past the edge by the margin, where the edge fades out, and turns with the shape around its center, while the texture coordinates go from 0 to 1 across the rectangle of the shape, upright on a flipped texture. The flags of the form say whether the shape is an ellipse and whether its texture is flipped. The fragment stage measures from the center in the frame of the shape, where the border and the radii of the corners take their shares of the shorter half side, and from the same point turned so the sector lies around the x axis. The edge fades over the softness, or over one pixel, the mean of how far the point of the shape moves from one pixel to the next across and down, which the canvas fixes for the whole shape since it maps the canvas to the pixels without bending it.
    vec2 local = (corner * 2.0 - 1.0) * (shape_half_size + shape_reach.y);
    float sine = sin(shape_rotation);
    float cosine = cos(shape_rotation);
    vec2 world = shape_center + vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
    gl_Position = view_projection * vec4(world, 0.0, 1.0);
    haylen_output_point = world;
    float flags = floor(shape_form.w * 65535.0 + 0.5);
    uv = local / shape_half_size * 0.5 + 0.5;
    if (flags >= 2.0) {
        uv.y = 1.0 - uv.y;
    }
    color = shape_color;
    haylen_flash = vec4(0.0);
    haylen_text_style = vec3(0.0);
    haylen_transform = vec2(shape_rotation, 0.0);
    haylen_source = vec4(0.0, 0.0, 1.0, 1.0);
    haylen_data = vec4(0.0);
    float shorter = min(shape_half_size.x, shape_half_size.y);
    float middle = (shape_form.z + shape_form.y * 0.5) * 6.28318531;
    haylen_shape_frame = vec4(local, shape_half_size);
    haylen_shape_sector_point = vec4(local.x * cos(middle) + local.y * sin(middle), local.y * cos(middle) - local.x * sin(middle), shape_form.y, 0.0);
    vec2 across = 0.5 * haylen_viewport.x * vec2(view_projection[0].x, view_projection[1].x);
    vec2 down = 0.5 * haylen_viewport.y * vec2(view_projection[0].y, view_projection[1].y);
    float pixel = 0.5 * (length(across) + length(down)) / max(abs(across.x * down.y - across.y * down.x), 0.000001);
    haylen_shape_form = vec4(shape_form.x * shorter, max(max(pixel, shape_reach.x), 0.000001), shorter, mod(flags, 2.0));
    haylen_shape_radii = shape_radii * shorter;
    haylen_shape_border = shape_border_color;
#else
    // The skew leans the quad around its pivot, which glyphs place on their baseline, before the rotation turns it.
    float angle = instance_rotation;
    vec2 local = (corner - instance_pivot) * instance_size;
    local.x -= haylen_signed_byte(instance_parameters.z) / 127.0 * local.y;
    float sine = sin(angle);
    float cosine = cos(angle);
    vec2 turned = vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
#ifndef HAYLEN_TEXT
    // The tilt lays the turned quad on a plane that leans away from the viewer, which shortens its height on the screen.
    turned.y *= 1.0 - instance_parameters.w;
#endif
    vec2 world = instance_position + turned;
    gl_Position = view_projection * vec4(world, 0.0, 1.0);
    haylen_output_point = world;

    // Text reads the outline, the spread of a synthetic bold and the softness from its parameters and takes the rectangle of its glyph as it is, since glyphs never flip or repeat. Sprites read flip flags that follow Tiled: bit 0 flips horizontally, bit 1 vertically and bit 2 swaps the axes. Tiled swaps the image first and flips the result, so the lookup flips first and swaps last.
#ifdef HAYLEN_TEXT
    float flags = 0.0;
    haylen_text_style = vec3(instance_parameters.x * 0.5, instance_parameters.y * 0.25, instance_parameters.w * 0.5);
    vec2 texel = corner;
    vec4 source = instance_uv;
#else
    float flags = floor(instance_parameters.x * 255.0 + 0.5);
    haylen_text_style = vec3(0.0);
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
    vec4 source = haylen_instance_source(flags);
#endif
    uv = mix(source.xy, source.zw, texel);
    haylen_source = vec4(min(source.xy, source.zw), max(source.xy, source.zw));
    color = instance_color;
    haylen_flash = instance_flash;
    haylen_transform = vec2(angle, flags);
#ifdef HAYLEN_BATCH
    haylen_data = instance_data;
    haylen_texture_index = instance_texture;
#else
    haylen_data = vec4(0.0);
#endif
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
#ifdef HAYLEN_BATCH
// The textures of a batch after its first, which share the sampler of the first one.
layout(binding=25) uniform texture2D sprite_texture_1;
layout(binding=26) uniform texture2D sprite_texture_2;
layout(binding=27) uniform texture2D sprite_texture_3;
layout(binding=28) uniform texture2D sprite_texture_4;
layout(binding=29) uniform texture2D sprite_texture_5;
layout(binding=30) uniform texture2D sprite_texture_6;
layout(binding=31) uniform texture2D sprite_texture_7;
#endif

in vec2 uv;
#ifdef HAYLEN_MESH
in vec4 color;
#else
flat in vec4 color;
#endif
flat in vec4 haylen_flash;
flat in vec3 haylen_text_style;
flat in vec2 haylen_transform;
flat in vec4 haylen_source;
flat in vec4 haylen_data;
#ifdef HAYLEN_BATCH
flat in float haylen_texture_index;
#endif

#ifdef HAYLEN_SHAPE
in vec4 haylen_shape_frame;
in vec4 haylen_shape_sector_point;
flat in vec4 haylen_shape_form;
flat in vec4 haylen_shape_radii;
flat in vec4 haylen_shape_border;

// The signed distance from a point in the frame of the shape to the edge of its ellipse, negative inside, exact at any ratio of its radii. A circle measures from its center. With the longer radius along x and the point in the first quarter, the nearest point of the edge is `(r x / (t + r - 1), y / t)` for the ratio `r` of the squared radii and the root `t` of the equation of the edge, between bounds that always hold it, while a point on the long axis has the nearest point in closed form. Four steps toward the nearest point along the circles of curvature of the edge, whose centers lie on the evolute of the ellipse, find the root closely at any ratio, and one Newton step on `1 / |q| - 1`, which is linear in the root for a circle, takes it to the precision of a float, so the distance holds within a fiftieth of a pixel for ellipses thousands of pixels long at any ratio.
float haylen_shape_ellipse(vec2 point) {
    if (haylen_shape_frame.z == haylen_shape_frame.w) {
        return length(point) - haylen_shape_frame.z;
    }

    bool tall = haylen_shape_frame.w > haylen_shape_frame.z;
    vec2 radii = tall ? haylen_shape_frame.wz : haylen_shape_frame.zw;
    vec2 at = abs(tall ? point.yx : point);
    vec2 scaled = at / radii;
    float level = dot(scaled, scaled) - 1.0;
    if (scaled.y <= 0.0) {
        float spread = radii.x * radii.x - radii.y * radii.y;
        if (radii.x * at.x >= spread) {
            return at.x - radii.x;
        }
        float share = radii.x * at.x / spread;
        return -length(vec2(radii.x * share - at.x, radii.y * sqrt(1.0 - share * share)));
    }

    float ratio = radii.x * radii.x / (radii.y * radii.y);
    float pull = ratio * scaled.x;
    float low = scaled.y;
    float high = level < 0.0 ? 1.0 : length(vec2(pull, scaled.y));
    float spread = radii.x * radii.x - radii.y * radii.y;
    vec2 reach = vec2(spread, -spread) / radii;
    vec2 turn = vec2(0.70710678);
    for (int iteration = 0; iteration < 4; iteration++) {
        vec2 center = reach * turn * turn * turn;
        vec2 toward = at - center;
        float curvature = length(radii * turn - center);
        turn = clamp((toward * (curvature / max(length(toward), 0.000001)) + center) / radii, 0.0, 1.0);
        turn /= max(length(turn), 0.000001);
    }
    float root = clamp(scaled.y / max(turn.y, 0.000001), low, high);
    float shifted = root + ratio - 1.0;
    vec2 part = vec2(pull / shifted, scaled.y / root);
    float size = length(part);
    root = clamp(root - size * size * (1.0 - size) / (part.x * part.x / shifted + part.y * part.y / root), low, high);
    float gap = length(vec2(ratio * at.x / (root + ratio - 1.0), at.y / root) - at);
    return level < 0.0 ? -gap : gap;
}

// The signed distance from a point in the frame of the shape to the edge of its rounded rectangle, negative inside, with the radius of the corner of the quarter the point lies in, or to the edge of its ellipse.
float haylen_shape_box(vec2 point) {
    if (haylen_shape_form.w > 0.5) {
        return haylen_shape_ellipse(point);
    }
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

// The signed distance from the center of the pixel to the edge of the shape, which its coverage and a material such as glass both read, so each pixel measures it once.
float haylen_shape_edge = 0.0;
bool haylen_shape_measured = false;

float haylen_shape_distance() {
    if (!haylen_shape_measured) {
        haylen_shape_edge = haylen_shape_box(haylen_shape_frame.xy);
        haylen_shape_measured = true;
    }
    return haylen_shape_edge;
}

// The coverage of the pixel, which the paint of a shape and its output both read, so each pixel measures it once.
vec2 haylen_shape_coverage = vec2(0.0);
bool haylen_shape_covered = false;

// How much of the pixel the shape covers, and how much of it its fill covers inside the border. Each edge fades over the ramp of the form, one pixel of the screen or the softness, so the coverage of a pixel follows how far its center lies inside the edge. The coverage past the far side of a thin shape is taken away again, so shapes thinner than a pixel cover only what they cross.
vec2 haylen_shape_cover() {
    if (haylen_shape_covered) {
        return haylen_shape_coverage;
    }

    float ramp = haylen_shape_form.y;
    float box = haylen_shape_distance();
    float sector = haylen_shape_sector();
    float beyond = clamp(0.5 - (box + 2.0 * haylen_shape_form.z) / ramp, 0.0, 1.0);
    float outer = clamp(0.5 - max(box, sector) / ramp, 0.0, 1.0) - beyond;
    float inner = clamp(0.5 - max(box + haylen_shape_form.x, sector) / ramp, 0.0, 1.0) - beyond;
    haylen_shape_coverage = vec2(max(outer, 0.0), max(inner, 0.0));
    haylen_shape_covered = true;
    return haylen_shape_coverage;
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

#ifdef HAYLEN_SHAPE
// What lies behind a glass draw at the pixel of the fragment moved by a shift in units of the canvas, which turns into pixels through how the point of the canvas changes from one pixel to the next. The texture of a glass draw is the image behind it with the size of its destination, so the pixel of the fragment reads it on every graphics API whichever way its rows run.
vec4 haylen_backdrop(vec2 shift) {
    vec2 across = dFdx(haylen_output_point);
    vec2 down = dFdy(haylen_output_point);
    float determinant = across.x * down.y - down.x * across.y;
    vec2 pixels = vec2(shift.x * down.y - down.x * shift.y, across.x * shift.y - shift.x * across.y) / (abs(determinant) > 0.0000001 ? determinant : 0.0000001);
    vec2 size = vec2(textureSize(sampler2D(sprite_texture, sprite_sampler), 0));
    return textureLod(sampler2D(sprite_texture, sprite_sampler), (gl_FragCoord.xy + pixels) / size, 0.0);
}
#endif

// The texture of the draw at a point, with the slopes of the point across one pixel to the right and one pixel down, which pick its mip level. A loop that samples takes the slopes once before it starts and passes them to every step, so compilers keep the loop instead of unrolling it to take the slopes at each step. A sprite of a batch with several textures picks its own texture by its index.
vec4 haylen_texture_grad(vec2 point, vec2 slopeX, vec2 slopeY) {
#ifdef HAYLEN_BATCH
    int index = int(haylen_texture_index + 0.5);
    if (index == 1) {
        return textureGrad(sampler2D(sprite_texture_1, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 2) {
        return textureGrad(sampler2D(sprite_texture_2, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 3) {
        return textureGrad(sampler2D(sprite_texture_3, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 4) {
        return textureGrad(sampler2D(sprite_texture_4, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 5) {
        return textureGrad(sampler2D(sprite_texture_5, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 6) {
        return textureGrad(sampler2D(sprite_texture_6, sprite_sampler), point, slopeX, slopeY);
    }
    if (index == 7) {
        return textureGrad(sampler2D(sprite_texture_7, sprite_sampler), point, slopeX, slopeY);
    }
    return textureGrad(sampler2D(sprite_texture, sprite_sampler), point, slopeX, slopeY);
#else
    if (haylen_texture_form > 0.5) {
        return textureLod(sampler2D(sprite_texture, sprite_sampler), point, 0.0);
    }
    return textureGrad(sampler2D(sprite_texture, sprite_sampler), point, slopeX, slopeY);
#endif
}

// The texture of the draw at a point, read at the mip level of the footprint of a pixel. A point that jumps between neighboring pixels, such as one clamped into the source, snapped to a cell or turned by a different angle in each pixel, has slopes far larger than the pixel covers, which would pick a tiny mip level and smear the texture. So the lookup takes the smaller of the slopes of the point and those of the texture coordinate of the draw, which hold the footprint of the pixel for a point shifted from it. A texture of a single level, such as the atlas of text, has no level to pick, so its lookups take no slopes. The index of the texture of a sprite of a batch is the same across the quad of the sprite, but compilers cannot tell, so the lookup takes the slopes before it picks the texture.
vec4 haylen_texture(vec2 point) {
#ifndef HAYLEN_BATCH
    if (haylen_texture_form > 0.5) {
        return textureLod(sampler2D(sprite_texture, sprite_sampler), point, 0.0);
    }
#endif
    vec2 pointX = dFdx(point);
    vec2 pointY = dFdy(point);
    vec2 drawX = dFdx(uv);
    vec2 drawY = dFdy(uv);
    bool steady = dot(pointX, pointX) + dot(pointY, pointY) <= dot(drawX, drawX) + dot(drawY, drawY);
    return haylen_texture_grad(point, steady ? pointX : drawX, steady ? pointY : drawY);
}

// The size in texels of the texture of the draw.
vec2 haylen_texture_size() {
#ifdef HAYLEN_BATCH
    int index = int(haylen_texture_index + 0.5);
    if (index == 1) {
        return vec2(textureSize(sampler2D(sprite_texture_1, sprite_sampler), 0));
    }
    if (index == 2) {
        return vec2(textureSize(sampler2D(sprite_texture_2, sprite_sampler), 0));
    }
    if (index == 3) {
        return vec2(textureSize(sampler2D(sprite_texture_3, sprite_sampler), 0));
    }
    if (index == 4) {
        return vec2(textureSize(sampler2D(sprite_texture_4, sprite_sampler), 0));
    }
    if (index == 5) {
        return vec2(textureSize(sampler2D(sprite_texture_5, sprite_sampler), 0));
    }
    if (index == 6) {
        return vec2(textureSize(sampler2D(sprite_texture_6, sprite_sampler), 0));
    }
    if (index == 7) {
        return vec2(textureSize(sampler2D(sprite_texture_7, sprite_sampler), 0));
    }
#endif
    return vec2(textureSize(sampler2D(sprite_texture, sprite_sampler), 0));
}

// The texels of the source rectangle of the draw, half a texel in from its sides, as the smallest corner in `xy` and the largest in `zw`.
vec4 haylen_source_texels() {
    vec2 margin = 0.5 / haylen_texture_size();
    vec2 middle = (haylen_source.xy + haylen_source.zw) * 0.5;
    return vec4(min(haylen_source.xy + margin, middle), max(haylen_source.zw - margin, middle));
}

// Moves a point inside the texels of the source rectangle of the draw, so filtering never reads the texels around it, such as the neighbours of a region of an atlas or the borders of a stretched nine-slice center. Meshes keep their texture coordinates, and shapes keep theirs inside the texture, so the fade past their edge never wraps around a repeating texture.
vec2 haylen_inside(vec2 point) {
#if defined(HAYLEN_MESH)
    return point;
#elif defined(HAYLEN_SHAPE)
    return clamp(point, 0.0, 1.0);
#else
    vec4 texels = haylen_source_texels();
    return clamp(point, texels.xy, texels.zw);
#endif
}

// A texel of the draw multiplied by the draw color and mixed toward the flash color by its alpha, which a shape paints with its border, since shapes never flash.
vec4 haylen_tint(vec4 texel) {
#ifdef HAYLEN_SHAPE
    return haylen_shape_paint(texel * color);
#else
    vec4 shaded = texel * color;
    shaded.rgb = mix(shaded.rgb, haylen_flash.rgb, haylen_flash.a);
    return shaded;
#endif
}

// The texture color inside the source of the draw at a point, tinted by the draw. A draw of the white texture, whose texels are all 1, skips the lookup.
vec4 haylen_sprite(vec2 point) {
    vec4 texel = vec4(1.0);
    if (haylen_texture_form < 1.5) {
        texel = haylen_texture(haylen_inside(point));
    }
    return haylen_tint(texel);
}

// The sprite at the texture coordinate of the draw, which is how sprites, meshes and shapes draw. The coordinate moves linearly across the draw and the move inside the source only stops it, so the texture takes the mip level from the coordinate itself, the level that `haylen_texture` picks for it, without the slopes and the lookup by gradients that a point which jumps needs. A draw of the white texture skips the lookup, and the several textures of a batch keep the lookup of `haylen_texture`, which picks one of them.
vec4 haylen_draw_sprite() {
    vec4 texel = vec4(1.0);
    if (haylen_texture_form < 1.5) {
#ifdef HAYLEN_BATCH
        texel = haylen_texture(haylen_inside(uv));
#else
        texel = texture(sampler2D(sprite_texture, sprite_sampler), haylen_inside(uv));
#endif
    }
    return haylen_tint(texel);
}

// The distance field of the glyph at a point, moved inside the texels of the source of the glyph. Glyphs join the atlas of the text programs while the app runs, so the atlas has a single level, which the lookups of a glyph read without the slopes that would pick one. The other programs read their texture like any lookup, and pass no texels.
float haylen_text_field(vec2 point, vec4 texels) {
#ifdef HAYLEN_TEXT
    return textureLod(sampler2D(sprite_texture, sprite_sampler), clamp(point, texels.xy, texels.zw), 0.0).r;
#else
    return haylen_texture(haylen_inside(point)).r;
#endif
}

// How much the distance of the field changes across one pixel of the screen, as `fwidth` measures it, taken from the field half a pixel to each side of the point. The point moves linearly across the quad, so its derivatives give the same pixel on GPUs that take derivatives once for each block of four pixels and on those that take them for each pixel, while the derivatives of the distance itself would differ between them.
float haylen_text_smoothing(vec2 point, vec4 texels) {
    vec2 across = 0.5 * dFdx(point);
    vec2 down = 0.5 * dFdy(point);
    float horizontal = haylen_text_field(point + across, texels) - haylen_text_field(point - across, texels);
    float vertical = haylen_text_field(point + down, texels) - haylen_text_field(point - down, texels);
    return abs(horizontal) + abs(vertical);
}

// A glyph of a text atlas, which stores a signed distance field whose edge is 0.5, filled with the draw color and outlined with the flash color. The text program of the engine compiles once more with `HAYLEN_TEXT_PLAIN` for the glyphs without an outline, a synthetic bold or softness, which most text is, since a modest GPU does the work of the outline and the softness for every glyph that a branch would skip. A synthetic bold spreads the glyph along its baseline by a share of its quad to each side, the union of the glyph moved left and moved right, so its strokes widen while the gaps above and below them, such as the one between the dot of an i and its stem, stay as open as the font draws them. The softness widens the edge into a blur for soft shadows and glows. With the smoothing of the edge on screen the outline and the softness shrink together to stop a sixteenth of the field short of 0, which the border of the quad of the glyph reaches, so a glyph never shows its quad at any size. The fill covers the outline in premultiplied colors, so each coverage counts once, and the result returns to straight colors.
vec4 haylen_text(vec2 point) {
#ifdef HAYLEN_TEXT
    vec4 texels = haylen_source_texels();
#else
    vec4 texels = vec4(0.0);
#endif
    float distance = haylen_text_field(point, texels);
#ifdef HAYLEN_TEXT_PLAIN
    float smoothing = max(haylen_text_smoothing(point, texels) * 0.7, 0.0001);
    return vec4(color.rgb, color.a * smoothstep(0.5 - smoothing, 0.5 + smoothing, distance));
#else
    if (haylen_text_style.y > 0.0) {
        vec2 spread = vec2(haylen_text_style.y * (haylen_source.z - haylen_source.x), 0.0);
        distance = max(haylen_text_field(point - spread, texels), haylen_text_field(point + spread, texels));
    }
    float smoothing = max(haylen_text_smoothing(point, texels) * 0.7, 0.0001);

    // A glyph without a visible outline and without softness is its fill alone.
    if (haylen_flash.a <= 0.0 && haylen_text_style.z <= 0.0) {
        return vec4(color.rgb, color.a * smoothstep(0.5 - smoothing, 0.5 + smoothing, distance));
    }

    float reach = haylen_text_style.x + haylen_text_style.z;
    float room = max(0.4375 - smoothing, 0.0);
    float fit = reach > room ? room / reach : 1.0;
    float outline = haylen_text_style.x * fit;
    float blur = smoothing + haylen_text_style.z * fit;
    float fill = smoothstep(0.5 - blur, 0.5 + blur, distance);
    float border = smoothstep(0.5 - outline - blur, 0.5 - outline + blur, distance);
    vec4 inner = vec4(color.rgb * color.a, color.a) * fill;
    vec4 outer = vec4(haylen_flash.rgb * haylen_flash.a, haylen_flash.a) * border;
    vec4 glyph = inner + outer * (1.0 - inner.a);
    return vec4(glyph.rgb / max(glyph.a, 0.0001), glyph.a);
#endif
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
