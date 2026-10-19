// The functions that the shaders of the library share: hashes and value noise for patterns that move, the luminance of a color and a hue turned into a color.
@block library_common
float library_hash(vec2 point) {
    return fract(sin(dot(point, vec2(127.1, 311.7))) * 43758.5453);
}

float library_noise(vec2 point) {
    vec2 cell = floor(point);
    vec2 offset = fract(point);
    vec2 blend = offset * offset * (3.0 - 2.0 * offset);
    float a = library_hash(cell);
    float b = library_hash(cell + vec2(1.0, 0.0));
    float c = library_hash(cell + vec2(0.0, 1.0));
    float d = library_hash(cell + vec2(1.0, 1.0));
    return mix(mix(a, b, blend.x), mix(c, d, blend.x), blend.y);
}

float library_fbm(vec2 point) {
    float sum = 0.0;
    float weight = 0.5;
    for (int octave = 0; octave < 4; octave++) {
        sum += weight * library_noise(point);
        point *= 2.03;
        weight *= 0.5;
    }
    return sum;
}

float library_luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 library_hue(float hue) {
    return clamp(abs(fract(hue + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0) - 1.0, 0.0, 1.0);
}

// The point inside the source rectangle of the draw as a share from 0 to 1 on each axis.
vec2 library_local(vec2 point) {
    return (point - haylen_source.xy) / max(haylen_source.zw - haylen_source.xy, vec2(0.0001));
}

// A point given as a share of the source rectangle turned back into a texture coordinate.
vec2 library_texture_point(vec2 local) {
    return haylen_source.xy + local * (haylen_source.zw - haylen_source.xy);
}
@end
