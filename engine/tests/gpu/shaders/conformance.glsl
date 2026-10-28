// The programs of the conformance tests of the GPU devices, which draw flat colors, sample every type of texture and write several colors, so each test reads back what a device made of one feature of the contract.

// Triangles of the vertex buffer, moved and tinted by the uniforms of each draw, at the depth of each vertex.
@vs color_vs
layout(binding=0) uniform color_params {
    vec4 offset;
    vec4 tint;
};

in vec3 position;
in vec4 color0;
out vec4 color;

void main() {
    color = color0 * tint;
    gl_Position = vec4(position.xy + offset.xy, position.z, 1.0);
}
@end

// The same triangles, placed again by the offset of each instance.
@vs instanced_vs
in vec3 position;
in vec4 color0;
in vec2 instance_offset;
out vec4 color;

void main() {
    color = color0;
    gl_Position = vec4(position.xy + instance_offset, position.z, 1.0);
}
@end

@fs color_fs
in vec4 color;
out vec4 frag_color;

void main() {
    frag_color = color;
}
@end

// One triangle that covers the viewport in one color at one depth, without a vertex buffer.
@vs plane_vs
layout(binding=0) uniform plane_params {
    vec4 plane_color;
    vec4 plane_depth;
};

out vec4 tint;

void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, plane_depth.x, 1.0);
    tint = plane_color;
}
@end

@fs plane_fs
in vec4 tint;
out vec4 frag_color;

void main() {
    frag_color = tint;
}
@end

// One triangle that covers the viewport with texture coordinates whose origin is the top-left corner.
@vs fullscreen_vs
out vec2 uv;

void main() {
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
    uv = vec2(corner.x, 1.0 - corner.y);
}
@end

// The texture parameters: x is the mip level to read, y the layer of an array, the slice of a volume or the face of a cube, z the depth a comparison tests, and w the scale of the texture coordinates of a flat texture, which reach past 1 to show how the sampler wraps.
@block sample_params
layout(binding=0) uniform sample_params {
    vec4 lookup;
};
@end

@fs textured_fs
@include_block sample_params
layout(binding=0) uniform texture2D color_texture;
layout(binding=0) uniform sampler color_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = textureLod(sampler2D(color_texture, color_sampler), uv * lookup.w, lookup.x);
}
@end

@fs array_fs
@include_block sample_params
layout(binding=0) uniform texture2DArray layers_texture;
layout(binding=0) uniform sampler layers_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = textureLod(sampler2DArray(layers_texture, layers_sampler), vec3(uv, lookup.y), lookup.x);
}
@end

@fs volume_fs
@include_block sample_params
layout(binding=0) uniform texture3D volume_texture;
layout(binding=0) uniform sampler volume_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = textureLod(sampler3D(volume_texture, volume_sampler), vec3(uv, lookup.y), lookup.x);
}
@end

// Reads the middle of the face of a cube in the order +X, -X, +Y, -Y, +Z and -Z. The direction comes from arithmetic and not from a constant array, since the OpenGL ES translator of the Android emulator on SwiftShader reads one element of a constant array whatever index a shader computes.
@fs cube_fs
@include_block sample_params
layout(binding=0) uniform textureCube cube_texture;
layout(binding=0) uniform sampler cube_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    int face = int(lookup.y + 0.5);
    float facing = face % 2 == 0 ? 1.0 : -1.0;
    vec3 direction = vec3(face / 2 == 0 ? facing : 0.0, face / 2 == 1 ? facing : 0.0, face / 2 == 2 ? facing : 0.0);
    frag_color = textureLod(samplerCube(cube_texture, cube_sampler), direction, lookup.x);
}
@end

@fs unfilterable_fs
@include_block sample_params
@sample_type float_texture unfilterable_float
@sampler_type float_sampler nonfiltering
layout(binding=0) uniform texture2D float_texture;
layout(binding=0) uniform sampler float_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    frag_color = textureLod(sampler2D(float_texture, float_sampler), uv, 0.0);
}
@end

// Writes white where the depth of the texture passes the comparison of the sampler against the depth of the parameters, and black elsewhere.
@fs shadow_fs
@include_block sample_params
@sample_type depth_texture depth
layout(binding=0) uniform texture2D depth_texture;
layout(binding=0) uniform samplerShadow depth_sampler;

in vec2 uv;
out vec4 frag_color;

void main() {
    float lit = texture(sampler2DShadow(depth_texture, depth_sampler), vec3(uv, lookup.z));
    frag_color = vec4(lit, lit, lit, 1.0);
}
@end

// Writes a color of its own into each of four targets.
@fs targets_fs
out vec4 first_color;
out vec4 second_color;
out vec4 third_color;
out vec4 fourth_color;

void main() {
    first_color = vec4(1.0, 0.0, 0.0, 1.0);
    second_color = vec4(0.0, 1.0, 0.0, 1.0);
    third_color = vec4(0.0, 0.0, 1.0, 1.0);
    fourth_color = vec4(1.0, 1.0, 1.0, 1.0);
}
@end

@program color color_vs color_fs
@program instanced instanced_vs color_fs
@program plane plane_vs plane_fs
@program textured fullscreen_vs textured_fs
@program array fullscreen_vs array_fs
@program volume fullscreen_vs volume_fs
@program cube fullscreen_vs cube_fs
@program unfilterable fullscreen_vs unfilterable_fs
@program shadow fullscreen_vs shadow_fs
@program targets fullscreen_vs targets_fs
