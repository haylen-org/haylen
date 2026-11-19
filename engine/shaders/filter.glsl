// Shrinks, blurs and copies images over the whole viewport.
@include screen.glsl

@fs filter_fs
layout(binding=0) uniform filter_fs_params {
    vec4 settings;
    vec4 offset;
};
layout(binding=0) uniform texture2D source_texture;
layout(binding=0) uniform sampler filter_sampler;

in vec2 uv;
out vec4 frag_color;

vec4 read_source(vec2 point) {
    return texture(sampler2D(source_texture, filter_sampler), settings.z > 0.5 ? vec2(point.x, 1.0 - point.y) : point);
}

// Settings: x is 0 to shrink the image to half its size, keeping only what passes the threshold in y when y is not negative, 1 to blur it along `offset.xy` or 2 to copy it, and z flips rows. A shrink averages the four texels around each point, half a texel away in `offset.zw`, and a blur weighs nine taps of a Gaussian curve.
void main() {
    if (settings.x > 1.5) {
        frag_color = read_source(uv);
        return;
    }
    if (settings.x < 0.5) {
        vec2 texel = offset.zw;
        vec4 sum = read_source(uv + vec2(-texel.x, -texel.y)) + read_source(uv + vec2(texel.x, -texel.y)) + read_source(uv + vec2(-texel.x, texel.y)) + read_source(uv + texel);
        vec4 average = sum * 0.25;
        if (settings.y >= 0.0) {
            float brightness = max(average.r, max(average.g, average.b));
            average *= max(brightness - settings.y, 0.0) / max(brightness, 0.0001);
        }
        frag_color = average;
        return;
    }
    vec4 sum = read_source(uv) * 0.2270270;
    sum += (read_source(uv + offset.xy) + read_source(uv - offset.xy)) * 0.1945946;
    sum += (read_source(uv + offset.xy * 2.0) + read_source(uv - offset.xy * 2.0)) * 0.1216216;
    sum += (read_source(uv + offset.xy * 3.0) + read_source(uv - offset.xy * 3.0)) * 0.0540540;
    sum += (read_source(uv + offset.xy * 4.0) + read_source(uv - offset.xy * 4.0)) * 0.0162162;
    frag_color = sum;
}
@end

@program filter screen_vs filter_fs
