@vs backdrop_vs
layout(binding=0) uniform backdrop_vs_params {
    mat4 view_projection;
    // The corner where the image starts in xy and the edge along its rows in zw.
    vec4 placement;
    // The edge along its columns in xy.
    vec4 columns;
};

in vec2 corner;
out vec2 uv;

void main() {
    gl_Position = view_projection * vec4(placement.xy + corner.x * placement.zw + corner.y * columns.xy, 0.0, 1.0);
    uv = corner;
}
@end

@fs backdrop_fs
layout(binding=1) uniform backdrop_fs_params {
    // Settings: x is the blend mode from 0 for overlay to 8 for exclusion, y the progress of the mask, z is 1 when the mask is reversed and w the opacity.
    vec4 settings;
    // Flips: x is 1 when the rows of the image run upward and y when those of the mask do.
    vec4 flips;
};
layout(binding=0) uniform texture2D image_texture;
layout(binding=1) uniform texture2D mask_texture;
layout(binding=2) uniform texture2D backdrop_texture;
layout(binding=0) uniform sampler backdrop_sampler;

in vec2 uv;
out vec4 frag_color;

vec3 screen(vec3 below, vec3 above) {
    return below + above - below * above;
}

vec3 hard_light(vec3 below, vec3 above) {
    return mix(below * 2.0 * above, screen(below, 2.0 * above - 1.0), step(0.5, above));
}

vec3 soft_light(vec3 below, vec3 above) {
    vec3 curve = mix(sqrt(below), ((16.0 * below - 12.0) * below + 4.0) * below, step(below, vec3(0.25)));
    return mix(below + (2.0 * above - 1.0) * (curve - below), below - (1.0 - 2.0 * above) * below * (1.0 - below), step(above, vec3(0.5)));
}

vec3 dodge(vec3 below, vec3 above) {
    return mix(min(vec3(1.0), below / max(1.0 - above, vec3(0.0001))), vec3(1.0), step(1.0, above)) * step(0.0001, below);
}

vec3 burn(vec3 below, vec3 above) {
    vec3 burned = 1.0 - min(vec3(1.0), (1.0 - below) / max(above, vec3(0.0001)));
    return mix(burned * step(0.0001, above), vec3(1.0), step(0.9999, below));
}

// The separable blend modes of compositing, on colors that are not premultiplied.
vec3 blend(vec3 below, vec3 above) {
    float mode = settings.x;
    if (mode < 0.5) {
        return hard_light(above, below);
    } else if (mode < 1.5) {
        return min(below, above);
    } else if (mode < 2.5) {
        return max(below, above);
    } else if (mode < 3.5) {
        return dodge(below, above);
    } else if (mode < 4.5) {
        return burn(below, above);
    } else if (mode < 5.5) {
        return hard_light(below, above);
    } else if (mode < 6.5) {
        return soft_light(below, above);
    } else if (mode < 7.5) {
        return abs(below - above);
    }
    return below + above - 2.0 * below * above;
}

// The image of the group lands on the image below as the blend mode mixes their colors where both cover, and as plain compositing where only one of them does. Both images hold premultiplied colors, and so does the result, which replaces the pixel below.
void main() {
    vec4 above = texture(sampler2D(image_texture, backdrop_sampler), flips.x > 0.5 ? vec2(uv.x, 1.0 - uv.y) : uv);
    float cover = texture(sampler2D(mask_texture, backdrop_sampler), flips.y > 0.5 ? vec2(uv.x, 1.0 - uv.y) : uv).a;
    cover = settings.z > 0.5 ? 1.0 - cover : cover;
    above *= mix(1.0, cover, settings.y) * settings.w;
    vec4 below = texelFetch(sampler2D(backdrop_texture, backdrop_sampler), ivec2(gl_FragCoord.xy), 0);

    vec3 aboveColor = above.a > 0.0 ? above.rgb / above.a : vec3(0.0);
    vec3 belowColor = below.a > 0.0 ? below.rgb / below.a : vec3(0.0);
    vec3 mixed = clamp(blend(belowColor, aboveColor), 0.0, 1.0);
    vec3 color = above.rgb * (1.0 - below.a) + below.rgb * (1.0 - above.a) + above.a * below.a * mixed;
    frag_color = vec4(color, above.a + below.a * (1.0 - above.a));
}
@end

@program backdrop backdrop_vs backdrop_fs
