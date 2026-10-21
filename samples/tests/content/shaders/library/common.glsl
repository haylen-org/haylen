// The functions that the shaders of the library share: hashes and value noise for patterns that move, the luminance of a color, a hue turned into a color and the glow of a shape.
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

// The coverage of the shape that the alpha of the texture draws around a point, averaged over a disc of `reach` texels on a spiral of samples whose weights fall off like a bell, which glows add as light, the way a bloom spreads a bright shape. Each pixel turns the spiral by its own angle, so the samples leave a fine grain instead of copies of the shape.
float library_glow(vec2 point, vec2 texel, float reach) {
    float sum = 0.0;
    float total = 0.0;
    float spin = library_hash(point * 4096.0) * 6.28318531;
    for (int index = 0; index < 48; index++) {
        float share = (float(index) + 0.5) / 48.0;
        float angle = float(index) * 2.39996323 + spin;
        float weight = exp(-3.0 * share);
        sum += haylen_texture(haylen_inside(point + vec2(cos(angle), sin(angle)) * sqrt(share) * reach * texel)).a * weight;
        total += weight;
    }
    return sum / total;
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
