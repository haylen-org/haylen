// Bends the image with a sine wave that travels over time, like heat haze or a flag.
@include haylen/material.glsl

@fs wave_fs
@include_block haylen_fragment
layout(binding=1) uniform wave_params {
    float time;
    float amplitude;
    float frequency;
    float speed;
};

void main() {
    vec2 point = uv;
    point.x += sin(uv.y * frequency + time * speed) * amplitude;
    point.y += cos(uv.x * frequency * 0.7 + time * speed * 0.8) * amplitude * 0.5;
    haylen_output(haylen_base(point));
}
@end

@program wave haylen_vs wave_fs
