// The parameters of the fragment stage of a draw. Its fade fades its alpha out toward the edges of the fades around it. Each of the eight edges covers a point by `x * point.x + y * point.y + offset`, clamped to 0 and 1, and the coverages of the edges multiply, where an edge that fades nothing covers every point with 1. The count of edges lets a draw without a fade skip it and a fade of four edges or fewer read only the first four. The form of its texture lets its lookups skip work. Every vertex stage of a program that reads the fade passes `haylen_output_point`, the point of the canvas it draws at.
@block haylen_draw
layout(binding=6) uniform haylen_draw_params {
    vec4 haylen_fade_x;
    vec4 haylen_fade_y;
    vec4 haylen_fade_offset;
    vec4 haylen_fade_more_x;
    vec4 haylen_fade_more_y;
    vec4 haylen_fade_more_offset;
    float haylen_fade_count;
    // 0 when the texture of the draw has mip levels or a batch picks one of several textures, 1 when it has a single level, which lookups read without slopes, and 2 for the white texture, whose texels are all 1.
    float haylen_texture_form;
};

in vec2 haylen_output_point;

float haylen_fade() {
    if (haylen_fade_count < 0.5) {
        return 1.0;
    }

    vec2 point = haylen_output_point;
    vec4 first = clamp(haylen_fade_x * point.x + haylen_fade_y * point.y + haylen_fade_offset, 0.0, 1.0);
    float fade = first.x * first.y * first.z * first.w;
    if (haylen_fade_count > 4.5) {
        vec4 second = clamp(haylen_fade_more_x * point.x + haylen_fade_more_y * point.y + haylen_fade_more_offset, 0.0, 1.0);
        fade *= second.x * second.y * second.z * second.w;
    }
    return fade;
}
@end

// Writes the result of a fragment shader, which a shape fades by how much of the pixel it covers and every draw by its fade. Lit canvases compile it with `HAYLEN_LIT`, which fills four images at once: the color, the emission, the surface normal with its specular strength, and the light mask, layer and shininess that the light pass reads. Every vertex stage of a program that writes through it passes `haylen_output_premultiply`, which is 1 when the blend mode of the draw expects colors premultiplied by their alpha.
@block haylen_output
@include_block haylen_draw
flat in float haylen_output_premultiply;

vec3 haylen_output_color(vec4 result) {
    return result.rgb * mix(1.0, result.a, haylen_output_premultiply);
}

#ifdef HAYLEN_LIT
layout(binding=7) uniform haylen_lit_params {
    // Surface: x is the specular strength, y the emission strength, z is 1 for unshaded draws, and w how the blend of the draw covers the surface and the info: 0 not at all, 1 by straight alpha and 2 by premultiplied alpha.
    vec4 haylen_surface;
    // Info: x is the light mask, y the layer shifted by 128 and z the shininess of a normal-mapped draw, all divided by 255, and w is 1 for draws that leave the emission alone, as multiplying ones do.
    vec4 haylen_info;
};

layout(location=0) out vec4 frag_color;
layout(location=1) out vec4 frag_emission;
layout(location=2) out vec4 frag_surface;
layout(location=3) out vec4 frag_info;

// Unshaded draws move their color into the emission, so the light map leaves them untouched, and the info image takes the data of a draw only where it covers more than half of the pixel. Every color of the pass blends with the blend of the draw, so a draw that must leave a color as it was writes nothing into it, which no blend of a lit pass changes the color with, and a premultiplied draw premultiplies the surface and the info it covers.
void haylen_output_surface(vec4 result, vec2 normal, float specular, float shininess) {
#ifdef HAYLEN_SHAPE
    result.a *= haylen_shape_cover().x;
#endif
    result.a *= haylen_fade();
    float unshaded = haylen_surface.z;
    vec3 color = haylen_output_color(result);
    frag_color = vec4(color * (1.0 - unshaded), result.a);
    frag_emission = haylen_info.w > 0.5 ? vec4(0.0) : vec4(color * max(unshaded, haylen_surface.y), result.a);
    vec4 surface = vec4(normal * 0.5 + 0.5, specular, result.a);
    vec4 info = vec4(haylen_info.xy, shininess, step(0.5, result.a));
    if (haylen_surface.w < 0.5) {
        surface = vec4(0.0);
        info = vec4(0.0);
    } else if (haylen_surface.w > 1.5) {
        surface.rgb *= surface.a;
        info.rgb *= info.a;
    }
    frag_surface = surface;
    frag_info = info;
}

void haylen_output(vec4 result) {
    haylen_output_surface(result, vec2(0.0), 0.0, 0.0);
}
#else
out vec4 frag_color;

void haylen_output(vec4 result) {
#ifdef HAYLEN_SHAPE
    result.a *= haylen_shape_cover().x;
#endif
    result.a *= haylen_fade();
    frag_color = vec4(haylen_output_color(result), result.a);
}
#endif
@end
