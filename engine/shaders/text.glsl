// Draws the glyphs of distance field fonts. With HAYLEN_TEXT_GRADIENT the record after each glyph in the instance stream holds the gradient of its fill: the axis and base that give the position along the gradient at a point of the canvas, and up to four stops.
@include haylen/material.glsl

@vs text_vs
@include_block haylen_vertex
#ifdef HAYLEN_TEXT_GRADIENT
in vec3 gradient_line;
in vec4 gradient_color_0;
in vec4 gradient_color_1;
in vec4 gradient_color_2;
in vec4 gradient_color_3;
in vec4 gradient_offsets;
out float gradient_position;
out vec4 gradient_stop_0;
out vec4 gradient_stop_1;
out vec4 gradient_stop_2;
out vec4 gradient_stop_3;
out vec4 gradient_stop_offsets;
#endif

void main() {
    haylen_vertex_main();
#ifdef HAYLEN_TEXT_GRADIENT
    gradient_position = dot(haylen_output_point, gradient_line.xy) + gradient_line.z;
    gradient_stop_0 = gradient_color_0;
    gradient_stop_1 = gradient_color_1;
    gradient_stop_2 = gradient_color_2;
    gradient_stop_3 = gradient_color_3;
    gradient_stop_offsets = gradient_offsets;
#endif
}
@end

@fs text_fs
@include_block haylen_fragment
#ifdef HAYLEN_TEXT_GRADIENT
in float gradient_position;
in vec4 gradient_stop_0;
in vec4 gradient_stop_1;
in vec4 gradient_stop_2;
in vec4 gradient_stop_3;
in vec4 gradient_stop_offsets;

// Moves the color toward the next stop by how far the position lies from the stop before it, where two stops at one offset switch colors there.
vec4 gradient_step(vec4 painted, vec4 stop, float from, float to) {
    float share = gradient_position >= to ? 1.0 : clamp((gradient_position - from) / max(to - from, 0.000001), 0.0, 1.0);
    return mix(painted, vec4(stop.rgb * stop.a, stop.a), share);
}

// Blends from stop to stop in premultiplied colors, so a stop that fades out never darkens its neighbours, where the first color holds before the first stop and the last one after the last stop.
vec4 gradient_color() {
    vec4 painted = vec4(gradient_stop_0.rgb * gradient_stop_0.a, gradient_stop_0.a);
    painted = gradient_step(painted, gradient_stop_1, gradient_stop_offsets.x, gradient_stop_offsets.y);
    painted = gradient_step(painted, gradient_stop_2, gradient_stop_offsets.y, gradient_stop_offsets.z);
    painted = gradient_step(painted, gradient_stop_3, gradient_stop_offsets.z, gradient_stop_offsets.w);
    return vec4(painted.rgb / max(painted.a, 0.0001), painted.a);
}
#endif

// A glyph with a gradient has no outline of its own, so the glyph that `haylen_text` returns is the color of the draw with the coverage of the glyph in its alpha, which takes the gradient by multiplying it.
void main() {
#ifdef HAYLEN_TEXT_GRADIENT
    vec4 glyph = haylen_text(uv);
    vec4 paint = gradient_color();
    haylen_output(vec4(glyph.rgb * paint.rgb, glyph.a * paint.a));
#else
    haylen_output(haylen_text(uv));
#endif
}
@end

@program text text_vs text_fs
