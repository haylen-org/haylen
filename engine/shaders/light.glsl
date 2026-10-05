// Draws the lights of a lit canvas as instances of one quad, each with its values as attributes that stay the same across its quad.
@vs light_vs
layout(binding=0) uniform light_vs_params {
    mat4 view_projection;
};

in vec2 corner;
// Area: xy is the center of the quad and zw its half size, turned by the rotation in w of the origin.
in vec4 instance_area;
in vec4 instance_color;
in vec4 instance_shape;
in vec4 instance_cone;
in vec4 instance_origin;
in vec4 instance_range;
in vec4 instance_shadow_color;
in vec4 instance_shadow_map;
in vec4 instance_shadow_axis;
out vec2 world;
out vec2 shape_uv;
flat out vec4 color;
flat out vec4 shape;
flat out vec4 cone;
flat out vec4 origin;
flat out vec4 range;
flat out vec4 shadow_color;
flat out vec4 shadow_map;
flat out vec4 shadow_axis;

void main() {
    vec2 local = (corner * 2.0 - 1.0) * instance_area.zw;
    float sine = sin(instance_origin.w);
    float cosine = cos(instance_origin.w);
    world = instance_area.xy + vec2(local.x * cosine - local.y * sine, local.x * sine + local.y * cosine);
    shape_uv = corner;
    color = instance_color;
    shape = instance_shape;
    cone = instance_cone;
    origin = instance_origin;
    range = instance_range;
    shadow_color = instance_shadow_color;
    shadow_map = instance_shadow_map;
    shadow_axis = instance_shadow_axis;
    gl_Position = view_projection * vec4(world, 0.0, 1.0);
}
@end

@fs light_fs
in vec2 world;
in vec2 shape_uv;
// Color: rgb is the light color times its intensity and a the blend mode, 0 to add, 1 to subtract and 2 to mix.
flat in vec4 color;
// Shape: x is the kind, 0 for point, 1 for spot and 2 for directional lights, y the radius, z the height and w the shadow row, or -1 without shadows.
flat in vec4 shape;
// Cone: xy is the direction and zw the cosines of half the inner and half the outer angle.
flat in vec4 cone;
// Origin: xy is the light position and z the item mask.
flat in vec4 origin;
// Range: xy is the lowest and highest layer, z the number of shadow samples and w the texels between them.
flat in vec4 range;
flat in vec4 shadow_color;
// Shadow map: x is its width in texels and y the depth bias.
flat in vec4 shadow_map;
// Shadow axis of directional lights: x and y place the first texel and the span across the light, z and w the start and span of depths along it.
flat in vec4 shadow_axis;
layout(binding=0) uniform texture2D shape_texture;
layout(binding=1) uniform texture2D surface_texture;
layout(binding=2) uniform texture2D info_texture;
layout(binding=3) uniform texture2D shadow_texture;
layout(binding=0) uniform sampler shape_sampler;
layout(binding=1) uniform sampler surface_sampler;
layout(binding=2) uniform sampler shadow_sampler;
@image_sample_type shadow_texture unfilterable_float
@sampler_type shadow_sampler nonfiltering

out vec4 frag_color;

const float PI = 3.14159265;

// The 1D shadow map of point and spot lights stores the nearest occluder of each direction around the light, and the map of directional lights the nearest occluder along the light at each point across it, both as a fraction of their range.
float shadow_amount(vec2 offset, float reach_distance) {
    int width = int(shadow_map.x);
    int row = int(shape.w);
    bool directional = shape.x > 1.5;
    float coordinate;
    float depth;
    if (directional) {
        vec2 across = vec2(-cone.y, cone.x);
        coordinate = (dot(world, across) - shadow_axis.x) / shadow_axis.y * shadow_map.x;
        depth = (dot(world, cone.xy) - shadow_axis.z) / shadow_axis.w;
    } else {
        coordinate = (atan(offset.y, offset.x) / (2.0 * PI) + 0.5) * shadow_map.x;
        depth = reach_distance / shape.y;
    }

    int samples = int(range.z);
    int reach = samples / 2;
    float blocked = 0.0;
    for (int tap = -6; tap <= 6; tap++) {
        if (tap < -reach || tap > reach) {
            continue;
        }
        int texel = int(floor(coordinate + float(tap) * range.w));
        texel = directional ? clamp(texel, 0, width - 1) : ((texel % width) + width) % width;
        float occluder = texelFetch(sampler2D(shadow_texture, shadow_sampler), ivec2(texel, row), 0).x;
        blocked += depth > occluder + shadow_map.y ? 1.0 : 0.0;
    }
    return blocked / float(samples);
}

void main() {
    ivec2 pixel = ivec2(gl_FragCoord.xy);
    vec4 info = texelFetch(sampler2D(info_texture, surface_sampler), pixel, 0);
    int mask = int(info.x * 255.0 + 0.5);
    float layer = floor(info.y * 255.0 + 0.5) - 128.0;
    if ((mask & int(origin.z)) == 0 || layer < range.x || layer > range.y) {
        discard;
    }

    float kind = shape.x;
    vec2 offset = world - origin.xy;
    float along = length(offset);
    vec4 shaped = kind > 1.5 ? vec4(1.0) : texture(sampler2D(shape_texture, shape_sampler), shape_uv);
    float falloff = shaped.a;
    if (kind > 0.5 && kind < 1.5) {
        falloff *= smoothstep(cone.w, cone.z, dot(offset / max(along, 0.0001), cone.xy));
    }

    // Normal-mapped pixels have a shininess and receive light by the angle it reaches them at, with a highlight toward the viewer.
    float diffuse = 1.0;
    float specular = 0.0;
    float shininess = info.z * 255.0;
    if (shininess > 0.5) {
        vec4 surface = texelFetch(sampler2D(surface_texture, surface_sampler), pixel, 0);
        vec2 planar = surface.xy * 2.0 - 1.0;
        vec3 normal = vec3(planar, sqrt(max(0.0, 1.0 - dot(planar, planar))));
        vec3 toward = normalize(kind > 1.5 ? vec3(-cone.xy, shape.z) : vec3(-offset, shape.z));
        diffuse = max(dot(normal, toward), 0.0);
        specular = surface.z * pow(max(dot(normal, normalize(toward + vec3(0.0, 0.0, 1.0))), 0.0), shininess);
    }

    float shadowed = shape.w >= 0.0 ? shadow_amount(offset, along) * shadow_color.a : 0.0;
    vec3 tint = color.rgb * shaped.rgb;
    if (color.a > 1.5) {
        frag_color = vec4(tint, clamp(falloff * (diffuse + specular) * (1.0 - shadowed), 0.0, 1.0));
        return;
    }
    vec3 lit = tint * falloff * (diffuse + specular);
    frag_color = vec4(mix(lit, shadow_color.rgb * falloff, shadowed), 1.0);
}
@end

@program light light_vs light_fs
