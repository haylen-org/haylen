// Writes the result of a fragment shader. Lit canvases compile it with HAYLEN_LIT, which fills four images at once: the color, the emission, the surface normal with its specular strength, and the light mask, layer and shininess that the light pass reads.
@block haylen_output
#ifdef HAYLEN_LIT
layout(binding=7) uniform haylen_lit_params {
    // Surface: x is the specular strength, y the emission strength and z is 1 for unshaded draws.
    vec4 haylen_surface;
    // Info: x is the light mask, y the layer shifted by 128 and z the shininess of a normal-mapped draw, all divided by 255.
    vec4 haylen_info;
};

layout(location=0) out vec4 frag_color;
layout(location=1) out vec4 frag_emission;
layout(location=2) out vec4 frag_surface;
layout(location=3) out vec4 frag_info;

// Unshaded draws move their color into the emission, so the light map leaves them untouched, and the info image takes the data of a draw only where it covers more than half of the pixel.
void haylen_output_surface(vec4 result, vec2 normal, float specular, float shininess) {
    float unshaded = haylen_surface.z;
    frag_color = vec4(result.rgb * (1.0 - unshaded), result.a);
    frag_emission = vec4(result.rgb * max(unshaded, haylen_surface.y), result.a);
    frag_surface = vec4(normal * 0.5 + 0.5, specular, result.a);
    frag_info = vec4(haylen_info.xy, shininess, step(0.5, result.a));
}

void haylen_output(vec4 result) {
    haylen_output_surface(result, vec2(0.0), 0.0, 0.0);
}
#else
out vec4 frag_color;

void haylen_output(vec4 result) {
    frag_color = result;
}
#endif
@end
