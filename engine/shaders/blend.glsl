@include haylen/output.glsl

@vs blend_vs
layout(binding=0) uniform blend_vs_params {
    mat4 view_projection;
    vec4 area;
};

in vec2 corner;
out vec2 uv;

void main() {
    gl_Position = view_projection * vec4(area.xy + corner * area.zw, 0.0, 1.0);
    uv = corner;
}
@end

@fs blend_fs
layout(binding=1) uniform blend_fs_params {
    // Settings: x is the pattern, y the progress, z is 1 when reversed and w the page turn angle.
    vec4 settings;
    // Shape: xy is the center, z the dissolve cell size and w the largest pixelate block.
    vec4 shape;
    vec4 color;
    // Image: xy is the pixel size of the first image, z the aspect of the area and w the flip flags, 1 for the first image and 2 for the second.
    vec4 image;
};
layout(binding=0) uniform texture2D from_texture;
layout(binding=1) uniform texture2D to_texture;
layout(binding=0) uniform sampler blend_sampler;

in vec2 uv;

@include_block haylen_output

const float PI = 3.14159265;
const float EDGE = 0.04;

vec4 sample_from(vec2 point) {
    vec2 flipped = mod(image.w, 2.0) >= 1.0 ? vec2(point.x, 1.0 - point.y) : point;
    return texture(sampler2D(from_texture, blend_sampler), flipped);
}

vec4 sample_to(vec2 point) {
    vec2 flipped = image.w >= 2.0 ? vec2(point.x, 1.0 - point.y) : point;
    return texture(sampler2D(to_texture, blend_sampler), flipped);
}

// Stretches the progress past the edge width so the first image is whole at 0 and the second one is whole at 1.
float reveal(float threshold, float progress, float edge) {
    return clamp((progress * (1.0 + edge) - threshold) / edge, 0.0, 1.0);
}

float hash(vec2 cell) {
    return fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 dissolve(float progress) {
    vec2 cell = floor(uv * image.xy / max(shape.z, 1.0));
    return mix(sample_from(uv), sample_to(uv), reveal(hash(cell), progress, EDGE));
}

vec4 pixelate(float progress) {
    float block = max(1.0, shape.w * (1.0 - abs(progress * 2.0 - 1.0)));
    vec2 point = clamp((floor(uv * image.xy / block) + 0.5) * block / image.xy, vec2(0.0), vec2(1.0));
    return progress < 0.5 ? sample_from(point) : sample_to(point);
}

vec4 radial(float progress) {
    // The hand starts at twelve o'clock and turns clockwise on screen, where y grows downward.
    vec2 offset = (uv - shape.xy) * vec2(image.z, 1.0);
    float angle = atan(offset.x, -offset.y);
    float turn = (angle < 0.0 ? angle + 2.0 * PI : angle) / (2.0 * PI);
    turn = settings.z > 0.5 ? 1.0 - turn : turn;
    return mix(sample_from(uv), sample_to(uv), reveal(turn, progress, 0.005));
}

vec4 iris(float progress) {
    vec2 scale = vec2(image.z, 1.0);
    vec2 center = shape.xy * scale;
    float reach = max(max(length(center), length(center - vec2(scale.x, 0.0))), max(length(center - vec2(0.0, 1.0)), length(center - scale)));
    float radius = reach * abs(progress * 2.0 - 1.0);
    float inside = 1.0 - smoothstep(radius - 0.004, radius, length(uv * scale - center));
    vec4 picture = progress < 0.5 ? sample_from(uv) : sample_to(uv);
    return mix(color, picture, inside);
}

vec4 page_turn(float progress) {
    // The page is measured along the axis opposite to its motion, from 0 at the edge that stays down to 1 at the edge that lifts first.
    vec2 axis = -vec2(cos(settings.w), sin(settings.w));
    float low = min(min(dot(vec2(0.0, 0.0), axis), dot(vec2(1.0, 0.0), axis)), min(dot(vec2(0.0, 1.0), axis), dot(vec2(1.0, 1.0), axis)));
    float high = max(max(dot(vec2(0.0, 0.0), axis), dot(vec2(1.0, 0.0), axis)), max(dot(vec2(0.0, 1.0), axis), dot(vec2(1.0, 1.0), axis)));
    float span = high - low;
    float along = (dot(uv, axis) - low) / span;

    // The page wraps around a cylinder whose fold line moves from the lifting edge until the whole page has turned past the other edge.
    float radius = 0.08;
    float fold = mix(1.0, -radius, progress);
    if (along > fold + radius) {
        float shadow = 1.0 - (1.0 - clamp((along - fold - radius) / 0.06, 0.0, 1.0)) * 0.45 * (1.0 - progress);
        return vec4(sample_to(uv).rgb * shadow, 1.0);
    }

    // Each point is covered by the highest layer of paper: the turned back lying flat, the back of the curl, the front of the curl or the flat page.
    float back = 2.0 * fold - along + PI * radius;
    float front = along;
    float shade = 1.0;
    if (along > fold) {
        float bend = asin(clamp((along - fold) / radius, 0.0, 1.0));
        back = fold + radius * (PI - bend);
        front = fold + radius * bend;
        shade = 1.0 - 0.35 * sin(bend);
    }
    if (back <= 1.0) {
        vec3 paper = mix(sample_from(uv + axis * (back - along) * span).rgb, vec3(0.92), 0.75);
        return vec4(paper * (0.85 + 0.15 * shade), 1.0);
    }
    if (front <= 1.0) {
        return vec4(sample_from(uv + axis * (front - along) * span).rgb * shade, 1.0);
    }
    return sample_to(uv);
}

void main() {
    float progress = settings.y;
    if (settings.x < 0.5) {
        haylen_output(dissolve(progress));
    } else if (settings.x < 1.5) {
        haylen_output(pixelate(progress));
    } else if (settings.x < 2.5) {
        haylen_output(radial(progress));
    } else if (settings.x < 3.5) {
        haylen_output(iris(progress));
    } else {
        haylen_output(page_turn(progress));
    }
}
@end

@program blend blend_vs blend_fs
