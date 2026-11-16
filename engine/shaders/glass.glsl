@include haylen/material.glsl

@fs glass_fs
@include_block haylen_fragment
layout(binding=1) uniform glass_fs_params {
    // Look: x is the saturation, y the brightness, z the distance of the refraction and w the strength of the rim, with distances in units of the canvas.
    vec4 look;
    // The tint, mixed over the image behind by its alpha.
    vec4 tint;
    // Rim: x is the width of the band of the rim.
    vec4 rim;
};

// The direction away from the edge at a point of the frame of the shape, from the rounded corner the point lies by or the side it lies nearest to, or across the ellipse.
vec2 glass_normal(vec2 point) {
    vec2 side = vec2(point.x < 0.0 ? -1.0 : 1.0, point.y < 0.0 ? -1.0 : 1.0);
    vec2 half_size = haylen_shape_frame.zw;
    if (haylen_shape_form.w > 0.5) {
        vec2 slope = point / max(half_size * half_size, vec2(0.000001));
        return dot(slope, slope) > 0.0 ? normalize(slope) : vec2(0.0, -1.0);
    }
    vec4 radii = haylen_shape_radii;
    float radius = point.x < 0.0 ? (point.y < 0.0 ? radii.x : radii.w) : (point.y < 0.0 ? radii.y : radii.z);
    vec2 corner = abs(point) - half_size + radius;
    if (corner.x > 0.0 && corner.y > 0.0) {
        return normalize(corner) * side;
    }
    return corner.x > corner.y ? vec2(side.x, 0.0) : vec2(0.0, side.y);
}

// Glass shows the image behind its shape, bent outward near the edge by the refraction, graded and tinted, with the rim of light over it, and leaves the edge to the coverage of the shape. The image behind holds premultiplied colors, so where it is transparent the glass shows only its tint and its rim.
void main() {
    vec2 point = haylen_shape_frame.xy;
    float depth = -haylen_shape_box(point);
    float turn = haylen_transform.x;
    vec2 local = glass_normal(point);
    vec2 normal = vec2(local.x * cos(turn) - local.y * sin(turn), local.x * sin(turn) + local.y * cos(turn));

    float refraction = look.z;
    float band = min(refraction * 3.0, haylen_shape_form.z);
    float bend = band > 0.0 ? clamp(1.0 - depth / band, 0.0, 1.0) : 0.0;
    vec4 behind = haylen_backdrop(normal * refraction * bend * bend);

    vec3 straight = behind.rgb / max(behind.a, 0.0001);
    float gray = dot(straight, vec3(0.2126, 0.7152, 0.0722));
    straight = max(mix(vec3(gray), straight, look.x) * look.y, vec3(0.0));
    vec4 glass = vec4(straight * behind.a, behind.a);
    glass = vec4(tint.rgb * tint.a, tint.a) + glass * (1.0 - tint.a);

    float pixel = 0.5 * (length(dFdx(point)) + length(dFdy(point)));
    float width = max(rim.x, pixel);
    float facing = dot(normal, vec2(-0.70710678, -0.70710678));
    float catches = clamp(0.2 + 0.8 * max(facing, 0.0) + 0.4 * max(-facing, 0.0), 0.0, 1.0);
    float light = look.w * catches * (1.0 - smoothstep(0.0, width, depth));
    glass = vec4(light) + glass * (1.0 - light);

    vec4 fill = vec4(glass.rgb / max(glass.a, 0.0001), glass.a) * color;
    haylen_output(haylen_shape_paint(fill));
}
@end

@program glass haylen_vs glass_fs
