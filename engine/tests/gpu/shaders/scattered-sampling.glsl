// A material that reads its texture at a point that jumps from one pixel to the next, as glows, blurs and noise effects do: each texel of the draw reads a texel at an even column and row of the texture, picked by a hash of the texel.
@include haylen/material.glsl

@fs scattered_fs
@include_block haylen_fragment

void main() {
    vec2 size = vec2(64.0);
    vec2 cell = floor(uv * size);
    vec2 hashed = fract(sin(vec2(dot(cell, vec2(12.9898, 78.233)), dot(cell, vec2(39.3468, 11.135)))) * 43758.5453);
    vec2 picked = floor(hashed * size * 0.5) * 2.0;
    haylen_output(haylen_texture((picked + 0.5) / size));
}
@end

@program scattered haylen_vs scattered_fs
