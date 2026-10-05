@vs composite_vs
out vec2 uv;

void main() {
    // A single triangle covers the viewport with texture coordinates whose origin is the top-left corner.
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(corner.x, 1.0 - corner.y);
}
@end

@fs composite_fs
layout(binding=0) uniform composite_fs_params {
    vec4 tint;
    vec4 fade;
    vec4 grading;
    vec4 vignette;
    vec4 flags;
    vec4 view;
    vec4 shape;
};
layout(binding=0) uniform texture2D scene_texture;
layout(binding=1) uniform texture2D light_texture;
layout(binding=2) uniform texture2D emission_texture;
layout(binding=3) uniform texture2D distortion_texture;
layout(binding=4) uniform texture2D bloom_texture;
layout(binding=5) uniform texture2D lut_texture;
layout(binding=0) uniform sampler composite_sampler;

in vec2 uv;
out vec4 frag_color;

// Flags: x enables the light map and the emission, y flips rows for backends whose render targets start at the bottom, z picks the stage, 0 for everything, 1 for the image before bloom and blur and 2 for the rest, and w enables the distortion map.
vec2 source_point(vec2 point) {
    return flags.y > 0.5 ? vec2(point.x, 1.0 - point.y) : point;
}

// The scene holds colors premultiplied by its coverage, which stays below 1 where a transparent window lets the desktop through, and emitted light also covers what lies behind a transparent window, the way an additive draw does.
vec4 lit_scene(vec2 point) {
    vec2 at = source_point(point);
    vec4 scene = texture(sampler2D(scene_texture, composite_sampler), at);
    if (flags.x > 0.5) {
        vec3 emission = texture(sampler2D(emission_texture, composite_sampler), at).rgb;
        scene.rgb = scene.rgb * texture(sampler2D(light_texture, composite_sampler), at).rgb + emission;
        scene.a = max(scene.a, max(emission.r, max(emission.g, emission.b)));
    }
    return scene;
}

// The image before bloom and blur: pixelated into blocks of `shape.x` units, moved down the slope of the coverage of the distortion draws by up to `view.z` units, and split into its colors by `view.w` units toward the corners.
vec4 first_stage() {
    vec2 point = uv;
    if (shape.x > 0.0) {
        vec2 cells = max(view.xy / shape.x, vec2(1.0));
        point = (floor(point * cells) + 0.5) / cells;
    }
    if (flags.w > 0.5) {
        // The slope of the coverage over 8 units, which is 1 where a draw goes from nothing to full coverage within that distance.
        vec2 reach = 4.0 / view.xy;
        float right = texture(sampler2D(distortion_texture, composite_sampler), source_point(point + vec2(reach.x, 0.0))).a;
        float left = texture(sampler2D(distortion_texture, composite_sampler), source_point(point - vec2(reach.x, 0.0))).a;
        float below = texture(sampler2D(distortion_texture, composite_sampler), source_point(point + vec2(0.0, reach.y))).a;
        float above = texture(sampler2D(distortion_texture, composite_sampler), source_point(point - vec2(0.0, reach.y))).a;
        point -= vec2(right - left, below - above) * view.z / view.xy;
    }
    if (view.w <= 0.0) {
        return lit_scene(point);
    }
    vec2 shift = (uv - 0.5) * 2.0 * view.w / view.xy;
    vec4 red = lit_scene(point + shift);
    vec4 green = lit_scene(point);
    vec4 blue = lit_scene(point - shift);
    return vec4(red.r, green.g, blue.b, max(green.a, max(red.a, blue.a)));
}

// Looks the color up in a lookup texture of `shape.y` square cells side by side, one cell for each step of blue, with red across a cell and green down it.
vec3 graded(vec3 color) {
    float cells = shape.y;
    vec3 clamped = clamp(color, vec3(0.0), vec3(1.0));
    float slice = clamped.b * (cells - 1.0);
    float lower = floor(slice);
    float upper = min(lower + 1.0, cells - 1.0);
    vec2 inside = vec2((clamped.r * (cells - 1.0) + 0.5) / (cells * cells), (clamped.g * (cells - 1.0) + 0.5) / cells);
    vec3 first = texture(sampler2D(lut_texture, composite_sampler), inside + vec2(lower / cells, 0.0)).rgb;
    vec3 second = texture(sampler2D(lut_texture, composite_sampler), inside + vec2(upper / cells, 0.0)).rgb;
    return mix(first, second, slice - lower);
}

void main() {
    vec4 image = flags.z > 1.5 ? texture(sampler2D(scene_texture, composite_sampler), source_point(uv)) : first_stage();
    if (flags.z > 0.5 && flags.z < 1.5) {
        frag_color = image;
        return;
    }
    vec3 color = image.rgb;
    float alpha = image.a;

    // Bloom adds the bright parts of the image, blurred, by `vignette.w`.
    if (vignette.w > 0.0) {
        vec3 glow = texture(sampler2D(bloom_texture, composite_sampler), source_point(uv)).rgb * vignette.w;
        color += glow;
        alpha = max(alpha, max(glow.r, max(glow.g, glow.b)));
    }

    // Grading: x is saturation, y is brightness and z is contrast, which pivots on the middle gray of the coverage, and w mixes in the lookup texture.
    float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luminance), color, grading.x);
    color = (color - 0.5 * alpha) * grading.z + 0.5 * alpha;
    color *= grading.y;
    color *= tint.rgb;
    if (grading.w > 0.0 && alpha > 0.0) {
        color = mix(color, graded(color / alpha) * alpha, grading.w);
    }

    // Vignette: x is strength, y is inner radius and z is softness.
    float distance_to_center = length(uv - 0.5) * 1.41421356;
    float shade = smoothstep(vignette.y, vignette.y + vignette.z, distance_to_center);
    color *= 1.0 - shade * vignette.x;

    // The fade covers the canvas with an opaque color.
    color = mix(color, fade.rgb, fade.a);
    alpha = min(mix(alpha, 1.0, fade.a), 1.0);
    frag_color = vec4(clamp(color, vec3(0.0), vec3(alpha)), alpha);
}
@end

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

// Settings: x is 0 to shrink the image to half its size, keeping only what passes the threshold in y when y is not negative, or 1 to blur it along `offset.xy`, and z flips rows. A shrink averages the four texels around each point, half a texel away in `offset.zw`, and a blur weighs nine taps of a Gaussian curve.
void main() {
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

@program composite composite_vs composite_fs
@program filter composite_vs filter_fs
