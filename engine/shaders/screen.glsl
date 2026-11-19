// The vertex stage of the programs that cover the viewport with one triangle, such as compositing and the filters of images.
@vs screen_vs
layout(binding=1) uniform screen_vs_params {
    // The columns of the turn of the positions in clip space, which the turned images of a swapchain pass need and which is the identity elsewhere.
    vec4 turn;
};

out vec2 uv;

void main() {
    // A single triangle covers the viewport with texture coordinates whose origin is the top-left corner.
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(mat2(turn.xy, turn.zw) * (corner * 2.0 - 1.0), 0.0, 1.0);
    uv = vec2(corner.x, 1.0 - corner.y);
}
@end
