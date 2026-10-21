#include "backends/sokol/ImageReadback.hpp"

#if defined(SOKOL_D3D11)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <d3d11.h>
#include <wrl/client.h>
#elif defined(SOKOL_WGPU)
#include <webgpu/webgpu.h>

#include <atomic>
#elif defined(SOKOL_GLES3) && defined(__EMSCRIPTEN__)
#include <GLES3/gl3.h>
#include <webgl/webgl2.h>
#elif defined(SOKOL_GLES3)
#include <GLES3/gl3.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <GL/gl.h>
#else
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#endif

#include <cstring>
#include <stdexcept>

namespace haylen::sokol {

#if defined(SOKOL_D3D11)
// The staging texture takes a copy of the image, which the CPU maps once the GPU finished it.
struct ImageReadback::Pending {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
};
#elif defined(SOKOL_WGPU)
// The buffer takes a copy of the image in rows aligned for WebGPU, and the callback of its mapping marks the copy done. The flag outlives a readback that ends first.
struct ImageReadback::Pending {
    WGPUBuffer buffer = nullptr;
    std::uint32_t rowBytes = 0;
    std::shared_ptr<std::atomic<int>> state = std::make_shared<std::atomic<int>>(0);
};
#elif defined(_WIN32) && !defined(SOKOL_GLES3)
// OpenGL on Windows reaches every function past version 1.1 through the context, as sokol_gfx does.
struct ImageReadback::Pending {
    using GenObjects = void(APIENTRY*)(GLsizei, GLuint*);
    using DeleteObjects = void(APIENTRY*)(GLsizei, const GLuint*);
    using BindObject = void(APIENTRY*)(GLenum, GLuint);
    using AttachTexture = void(APIENTRY*)(GLenum, GLenum, GLenum, GLuint, GLint);
    using FillBuffer = void(APIENTRY*)(GLenum, std::ptrdiff_t, const void*, GLenum);
    using MapRange = void*(APIENTRY*)(GLenum, std::ptrdiff_t, std::ptrdiff_t, GLbitfield);
    using UnmapBuffer = GLboolean(APIENTRY*)(GLenum);
    using MakeFence = void*(APIENTRY*)(GLenum, GLbitfield);
    using WaitFence = GLenum(APIENTRY*)(void*, GLbitfield, std::uint64_t);
    using DeleteFence = void(APIENTRY*)(void*);

    static constexpr GLenum kReadFramebuffer = 0x8CA8;
    static constexpr GLenum kColorAttachment = 0x8CE0;
    static constexpr GLenum kPackBuffer = 0x88EB;
    static constexpr GLenum kStreamRead = 0x88E1;
    static constexpr GLenum kCommandsComplete = 0x9117;
    static constexpr GLenum kAlreadySignaled = 0x911A;
    static constexpr GLenum kConditionSatisfied = 0x911C;
    static constexpr GLbitfield kMapRead = 0x0001;

    template <typename Function> static Function load(const char* name) {
        return reinterpret_cast<Function>(reinterpret_cast<void*>(wglGetProcAddress(name)));
    }

    GenObjects genFramebuffers = load<GenObjects>("glGenFramebuffers");
    DeleteObjects deleteFramebuffers = load<DeleteObjects>("glDeleteFramebuffers");
    BindObject bindFramebuffer = load<BindObject>("glBindFramebuffer");
    AttachTexture framebufferTexture2D = load<AttachTexture>("glFramebufferTexture2D");
    GenObjects genBuffers = load<GenObjects>("glGenBuffers");
    DeleteObjects deleteBuffers = load<DeleteObjects>("glDeleteBuffers");
    BindObject bindBuffer = load<BindObject>("glBindBuffer");
    FillBuffer bufferData = load<FillBuffer>("glBufferData");
    MapRange mapBufferRange = load<MapRange>("glMapBufferRange");
    UnmapBuffer unmapBuffer = load<UnmapBuffer>("glUnmapBuffer");
    MakeFence fenceSync = load<MakeFence>("glFenceSync");
    WaitFence clientWaitSync = load<WaitFence>("glClientWaitSync");
    DeleteFence deleteSync = load<DeleteFence>("glDeleteSync");
    GLuint buffer = 0;
    void* fence = nullptr;
};
#else
// The pixel buffer takes the pixels that glReadPixels reads behind a fence, which the CPU maps once the GPU passed it.
struct ImageReadback::Pending {
    GLuint buffer = 0;
    GLsync fence = nullptr;
};
#endif

ImageReadback::ImageReadback(sg_image image, int imageWidth, int imageHeight) : width(imageWidth), height(imageHeight) {
    const auto size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
#if defined(SOKOL_D3D11)
    auto* device = static_cast<ID3D11Device*>(const_cast<void*>(sg_d3d11_device()));
    auto* context = static_cast<ID3D11DeviceContext*>(const_cast<void*>(sg_d3d11_device_context()));
    auto* source = static_cast<ID3D11Texture2D*>(const_cast<void*>(sg_d3d11_query_image_info(image).tex2d));
    D3D11_TEXTURE2D_DESC description{};
    source->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    pending = std::make_unique<Pending>();
    if (FAILED(device->CreateTexture2D(&description, nullptr, pending->staging.GetAddressOf()))) {
        throw std::runtime_error("The GPU found no memory to read the pixels of a render target into.");
    }
    context->CopyResource(pending->staging.Get(), source);
    static_cast<void>(size);
#elif defined(SOKOL_WGPU)
    const auto device = static_cast<WGPUDevice>(const_cast<void*>(sg_wgpu_device()));
    const auto queue = static_cast<WGPUQueue>(const_cast<void*>(sg_wgpu_queue()));
    const auto texture = static_cast<WGPUTexture>(const_cast<void*>(sg_wgpu_query_image_info(image).tex));
    pending = std::make_unique<Pending>();
    pending->rowBytes = (static_cast<std::uint32_t>(width) * 4U + 255U) / 256U * 256U;
    WGPUBufferDescriptor bufferDescription{};
    bufferDescription.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    bufferDescription.size = static_cast<std::uint64_t>(pending->rowBytes) * static_cast<std::uint64_t>(height);
    pending->buffer = wgpuDeviceCreateBuffer(device, &bufferDescription);
    if (pending->buffer == nullptr) {
        throw std::runtime_error("The GPU found no memory to read the pixels of a render target into.");
    }
    const WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, nullptr);
    WGPUTexelCopyTextureInfo from{};
    from.texture = texture;
    from.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo to{};
    to.buffer = pending->buffer;
    to.layout.bytesPerRow = pending->rowBytes;
    to.layout.rowsPerImage = static_cast<std::uint32_t>(height);
    const WGPUExtent3D extent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    wgpuCommandEncoderCopyTextureToBuffer(encoder, &from, &to, &extent);
    const WGPUCommandBuffer commands = wgpuCommandEncoderFinish(encoder, nullptr);
    wgpuQueueSubmit(queue, 1, &commands);
    wgpuCommandBufferRelease(commands);
    wgpuCommandEncoderRelease(encoder);

    // The state is 1 once mapped and 2 when the mapping failed, and the callback holds it until it runs.
    WGPUBufferMapCallbackInfo callback{};
    callback.mode = WGPUCallbackMode_AllowSpontaneous;
    // clang-format off
    callback.callback = [](WGPUMapAsyncStatus status, WGPUStringView, void* first, void*) {
        auto* held = static_cast<std::shared_ptr<std::atomic<int>>*>(first);
        (*held)->store(status == WGPUMapAsyncStatus_Success ? 1 : 2);
        delete held;
    };
    // clang-format on
    callback.userdata1 = new std::shared_ptr<std::atomic<int>>(pending->state);
    wgpuBufferMapAsync(pending->buffer, WGPUMapMode_Read, 0, static_cast<std::size_t>(bufferDescription.size), callback);
    static_cast<void>(size);
#else
    pending = std::make_unique<Pending>();
    const sg_gl_image_info info = sg_gl_query_image_info(image);
#if defined(_WIN32) && !defined(SOKOL_GLES3)
    Pending& gl = *pending;
    GLuint framebuffer = 0;
    gl.genFramebuffers(1, &framebuffer);
    gl.bindFramebuffer(Pending::kReadFramebuffer, framebuffer);
    gl.framebufferTexture2D(Pending::kReadFramebuffer, Pending::kColorAttachment, GL_TEXTURE_2D, info.tex[info.active_slot], 0);
    gl.genBuffers(1, &gl.buffer);
    gl.bindBuffer(Pending::kPackBuffer, gl.buffer);
    gl.bufferData(Pending::kPackBuffer, static_cast<std::ptrdiff_t>(size), nullptr, Pending::kStreamRead);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gl.fence = gl.fenceSync(Pending::kCommandsComplete, 0);
    gl.bindBuffer(Pending::kPackBuffer, 0);
    gl.bindFramebuffer(Pending::kReadFramebuffer, 0);
    gl.deleteFramebuffers(1, &framebuffer);
#else
    GLuint framebuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, info.tex[info.active_slot], 0);
    glGenBuffers(1, &pending->buffer);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pending->buffer);
    glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(size), nullptr, GL_STREAM_READ);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    pending->fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
#endif
    glFlush();
    sg_reset_state_cache();
#endif
}

ImageReadback::~ImageReadback() {
    if (!pending) {
        return;
    }
#if defined(SOKOL_WGPU)
    wgpuBufferRelease(pending->buffer);
#elif defined(_WIN32) && !defined(SOKOL_GLES3) && !defined(SOKOL_D3D11)
    pending->deleteSync(pending->fence);
    pending->deleteBuffers(1, &pending->buffer);
#elif !defined(SOKOL_D3D11)
    glDeleteSync(pending->fence);
    glDeleteBuffers(1, &pending->buffer);
#endif
}

std::optional<std::vector<std::uint8_t>> ImageReadback::poll() {
    const auto rowBytes = static_cast<std::size_t>(width) * 4U;
    std::vector<std::uint8_t> pixels(rowBytes * static_cast<std::size_t>(height), 0);
#if defined(SOKOL_D3D11)
    auto* context = static_cast<ID3D11DeviceContext*>(const_cast<void*>(sg_d3d11_device_context()));
    D3D11_MAPPED_SUBRESOURCE mapped{};
    const HRESULT result = context->Map(pending->staging.Get(), 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
    if (result == DXGI_ERROR_WAS_STILL_DRAWING) {
        return std::nullopt;
    }
    if (FAILED(result)) {
        throw std::runtime_error("The GPU could not hand over the pixels of a render target.");
    }
    for (int row = 0; row < height; ++row) {
        std::memcpy(&pixels[static_cast<std::size_t>(row) * rowBytes], static_cast<const std::uint8_t*>(mapped.pData) + static_cast<std::size_t>(row) * mapped.RowPitch, rowBytes);
    }
    context->Unmap(pending->staging.Get(), 0);
#elif defined(SOKOL_WGPU)
    const int state = pending->state->load();
    if (state == 0) {
        return std::nullopt;
    }
    if (state == 2) {
        throw std::runtime_error("The GPU could not hand over the pixels of a render target.");
    }
    const auto* mapped = static_cast<const std::uint8_t*>(wgpuBufferGetConstMappedRange(pending->buffer, 0, static_cast<std::size_t>(pending->rowBytes) * static_cast<std::size_t>(height)));
    for (int row = 0; row < height; ++row) {
        std::memcpy(&pixels[static_cast<std::size_t>(row) * rowBytes], mapped + static_cast<std::size_t>(row) * pending->rowBytes, rowBytes);
    }
    wgpuBufferUnmap(pending->buffer);
#elif defined(_WIN32) && !defined(SOKOL_GLES3)
    const GLenum status = pending->clientWaitSync(pending->fence, 0, 0);
    if (status != Pending::kAlreadySignaled && status != Pending::kConditionSatisfied) {
        return std::nullopt;
    }
    pending->bindBuffer(Pending::kPackBuffer, pending->buffer);
    const void* mapped = pending->mapBufferRange(Pending::kPackBuffer, 0, static_cast<std::ptrdiff_t>(pixels.size()), Pending::kMapRead);
    if (mapped != nullptr) {
        std::memcpy(pixels.data(), mapped, pixels.size());
    }
    pending->unmapBuffer(Pending::kPackBuffer);
    pending->bindBuffer(Pending::kPackBuffer, 0);
    sg_reset_state_cache();
#else
    const GLenum status = glClientWaitSync(pending->fence, 0, 0);
    if (status != GL_ALREADY_SIGNALED && status != GL_CONDITION_SATISFIED) {
        return std::nullopt;
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pending->buffer);
#if defined(__EMSCRIPTEN__)
    glGetBufferSubData(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(pixels.size()), pixels.data());
#else
    const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(pixels.size()), GL_MAP_READ_BIT);
    if (mapped != nullptr) {
        std::memcpy(pixels.data(), mapped, pixels.size());
    }
    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
#endif
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    sg_reset_state_cache();
#endif
    return pixels;
}

} // namespace haylen::sokol
