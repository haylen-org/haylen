#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(HAYLEN_GPU_DIRECT3D11)
#include "gpu/d3d11/Device.hpp"
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
#include "gpu/d3d12/Device.hpp"
#endif
#include "gpu/direct3d/Adapter.hpp"
#include "haylen/core/Log.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::gpu::direct3d {

// Runs the devices of Direct3D 11 and 12 on WARP, which every Windows machine has and which draws the same pixels on every one, with validation on, into windows that never show. A device logs what its validation finds, and any error fails the test.
class Direct3DDeviceTest : public ::testing::TestWithParam<Device::Api> {
  public:
    // The APIs of Direct3D that the build compiled.
    [[nodiscard]] static std::vector<Device::Api> getCompiledApis() {
        std::vector<Device::Api> apis;
#if defined(HAYLEN_GPU_DIRECT3D11)
        apis.push_back(Device::Api::Direct3D11);
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
        apis.push_back(Device::Api::Direct3D12);
#endif
        return apis;
    }

  protected:
    static constexpr int kSize = 8;

    static constexpr const char* kVertexSource = R"(
cbuffer params : register(b0) { float4 offset; };
float4 main(float2 position : TEXCOORD0) : SV_Position { return float4(position + offset.xy, 0.5, 1); }
)";
    static constexpr const char* kFragmentSource = R"(
cbuffer params : register(b0) { float4 color; };
float4 main() : SV_Target0 { return color; }
)";

    void SetUp() override {
        // clang-format off
        listener = core::Log::addListener([this](core::Log::Level level, std::string_view line) {
            if (level == core::Log::Level::Error) {
                errors.emplace_back(line);
            } else if (level == core::Log::Level::Warning) {
                warnings.emplace_back(line);
            }
        });
        // clang-format on
    }

    void TearDown() override {
        for (const Pipeline pipeline : pipelines) {
            device->destroy(pipeline);
        }
        for (const Program program : programs) {
            device->destroy(program);
        }
        for (const Buffer buffer : buffers) {
            device->destroy(buffer);
        }
        for (const Image image : images) {
            device->destroy(image);
        }
        device.reset();
        if (window != nullptr) {
            DestroyWindow(window);
        }
        core::Log::removeListener(listener);
        for (const std::string& error : errors) {
            ADD_FAILURE() << error;
        }
    }

    using Buffer = Device::Buffer;
    using Image = Device::Image;
    using Pipeline = Device::Pipeline;
    using Program = Device::Program;

    void start(const std::optional<Surface>& surface = std::nullopt) {
        Microsoft::WRL::ComPtr<IDXGIFactory6> factory = Adapter::createFactory(true);
        Microsoft::WRL::ComPtr<IDXGIAdapter1> warp = Adapter::getSoftware(*factory.Get());
#if defined(HAYLEN_GPU_DIRECT3D11)
        if (GetParam() == Device::Api::Direct3D11) {
            device = std::make_unique<d3d11::Device>(factory, warp, surface, true);
        }
#endif
#if defined(HAYLEN_GPU_DIRECT3D12)
        if (GetParam() == Device::Api::Direct3D12) {
            device = std::make_unique<d3d12::Device>(factory, warp, surface, true);
        }
#endif
    }

    // Starts the device on a window, or skips the test where Windows gives the process no desktop, as it does for a service.
    void startOnWindow(const Surface& surface) {
        try {
            start(surface);
        } catch (const std::runtime_error& error) {
            if (std::string(error.what()).find("no desktop") == std::string::npos) {
                throw;
            }
            GTEST_SKIP() << error.what();
        }
    }

    // A window that never shows, of the size, without a redirection bitmap when it is transparent.
    [[nodiscard]] Surface openWindow(int width, int height, bool transparent) {
        WNDCLASSW windowClass{};
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = L"HaylenDirect3DTest";
        RegisterClassW(&windowClass);
        const DWORD style = transparent ? WS_POPUP : WS_OVERLAPPEDWINDOW;
        window = CreateWindowExW(transparent ? WS_EX_NOREDIRECTIONBITMAP : 0U, windowClass.lpszClassName, L"Haylen", style, 0, 0, width, height, nullptr, nullptr, windowClass.hInstance, nullptr);
        if (window == nullptr) {
            throw std::runtime_error("The test could not open a window.");
        }
        return {.kind = Surface::Kind::Win32, .window = window, .display = windowClass.hInstance, .width = width, .height = height, .transparent = transparent};
    }

    [[nodiscard]] Pipeline makePipeline(bool swapchain) {
        const Device::Attribute attribute{.location = 0, .name = "position"};
        const std::array<Device::BlockBinding, 2> blocks{{{.slot = 0, .stage = Device::Stage::Vertex, .size = 16, .binding = 0}, {.slot = 1, .stage = Device::Stage::Fragment, .size = 16, .binding = 0}}};
        programs.push_back(device->makeProgram({.vertex = {.source = kVertexSource, .entry = "main", .profile = "vs_5_0"}, .fragment = {.source = kFragmentSource, .entry = "main", .profile = "ps_5_0"}, .attributes = std::span(&attribute, 1), .blocks = blocks, .label = "haylen-test-solid"}));
        const Device::VertexBufferLayout layout{.stride = 8};
        const Device::VertexAttribute vertexAttribute{.location = 0, .format = Device::VertexFormat::Float2};
        const Device::ColorTarget color;
        pipelines.push_back(device->makePipeline({.program = programs.back(), .buffers = std::span(&layout, 1), .attributes = std::span(&vertexAttribute, 1), .colors = std::span(&color, 1), .swapchain = swapchain, .label = "haylen-test-solid"}));
        return pipelines.back();
    }

    [[nodiscard]] Buffer makeTriangle(std::array<float, 6> corners) {
        buffers.push_back(device->makeBuffer({.data = std::as_bytes(std::span(corners)), .label = "haylen-test-triangle"}));
        return buffers.back();
    }

    void drawTriangle(Pipeline pipeline, Buffer vertices, std::array<float, 4> color) {
        device->applyPipeline(pipeline);
        Device::Bindings bindings;
        bindings.vertexBuffers[0] = vertices;
        device->applyBindings(bindings);
        const std::array<float, 4> offset{};
        device->applyUniforms(0, std::as_bytes(std::span(offset)));
        device->applyUniforms(1, std::as_bytes(std::span(color)));
        device->draw(0, 3, 1);
    }

    [[nodiscard]] std::vector<std::uint8_t> read(Image image) {
        const std::unique_ptr<Device::Readback> readback = device->readImage(image, kSize, kSize);
        for (int frame = 0; frame < 600; ++frame) {
            if (std::optional<std::vector<std::uint8_t>> pixels = readback->poll()) {
                return *pixels;
            }
            device->commit();
            Sleep(1);
        }
        throw std::runtime_error("The copy of the render target never finished.");
    }

    // Draws frames into the swapchain, with a pass that clears it and a triangle.
    void drawFrames(Pipeline pipeline, Buffer vertices, int count) {
        for (int frame = 0; frame < count; ++frame) {
            device->beginPass({.load = Device::Load::Clear, .clear = math::Color::black(), .label = "haylen-test-swapchain"});
            drawTriangle(pipeline, vertices, {1.0F, 0.0F, 0.0F, 1.0F});
            device->endPass();
            device->commit();
        }
    }

    std::unique_ptr<Device> device;
    std::vector<Pipeline> pipelines;
    std::vector<Program> programs;
    std::vector<Buffer> buffers;
    std::vector<Image> images;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::uint64_t listener = 0;
    HWND window = nullptr;
};

TEST_P(Direct3DDeviceTest, DrawsAndReadsBackOnWarp) {
    start();
    EXPECT_EQ(device->getApi(), GetParam());
    EXPECT_EQ(device->getLanguage(), Device::Language::Hlsl5);
    EXPECT_FALSE(device->getAdapterName().empty());
    images.push_back(device->makeImage({.width = kSize, .height = kSize, .kind = Device::ImageKind::RenderTarget, .label = "haylen-test-target"}));
    const Pipeline pipeline = makePipeline(false);
    const Buffer vertices = makeTriangle({-1.0F, -1.0F, -1.0F, 1.0F, 1.0F, -1.0F});

    const Device::ColorAttachment color{.image = images.back(), .clear = {0.0F, 0.0F, 1.0F, 1.0F}};
    device->beginPass({.colors = std::span(&color, 1)});
    drawTriangle(pipeline, vertices, {1.0F, 0.0F, 0.0F, 1.0F});
    device->endPass();
    device->commit();

    // The triangle covers the lower left half, and the readback starts with the top row.
    const std::vector<std::uint8_t> pixels = read(images.back());
    const std::size_t lowerLeft = (6 * kSize + 1) * 4;
    const std::size_t upperRight = (1 * kSize + 6) * 4;
    EXPECT_EQ((std::array{pixels[lowerLeft], pixels[lowerLeft + 1], pixels[lowerLeft + 2], pixels[lowerLeft + 3]}), (std::array<std::uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ((std::array{pixels[upperRight], pixels[upperRight + 1], pixels[upperRight + 2], pixels[upperRight + 3]}), (std::array<std::uint8_t, 4>{0, 0, 255, 255}));
}

TEST_P(Direct3DDeviceTest, PresentsThroughResizesMinimizingAndNewSurfaces) {
    Surface surface = openWindow(160, 90, false);
    surface.sampleCount = 4;
    startOnWindow(surface);
    if (IsSkipped()) {
        return;
    }
    const Pipeline pipeline = makePipeline(true);
    const Buffer vertices = makeTriangle({-0.5F, -0.5F, 0.0F, 0.5F, 0.5F, -0.5F});
    drawFrames(pipeline, vertices, 3);

    device->resizeSurface(200, 120);
    drawFrames(pipeline, vertices, 3);
    device->resizeSurface(0, 0);
    drawFrames(pipeline, vertices, 3);
    device->resizeSurface(160, 90);
    drawFrames(pipeline, vertices, 3);

    // A surface with another sample count makes the pipelines of the swapchain again.
    device->detachSurface();
    drawFrames(pipeline, vertices, 2);
    surface.sampleCount = 1;
    surface.vsync = false;
    device->attachSurface(surface);
    drawFrames(pipeline, vertices, 3);
    EXPECT_FALSE(device->isLost());
}

TEST_P(Direct3DDeviceTest, ComposesATransparentWindow) {
    startOnWindow(openWindow(160, 90, true));
    if (IsSkipped()) {
        return;
    }
    const Pipeline pipeline = makePipeline(true);
    const Buffer vertices = makeTriangle({-0.5F, -0.5F, 0.0F, 0.5F, 0.5F, -0.5F});
    drawFrames(pipeline, vertices, 3);
    device->resizeSurface(120, 60);
    drawFrames(pipeline, vertices, 3);
    EXPECT_FALSE(device->isLost());
}

// A warning of the debug layer reaches the log as an error, so the tests fail on it, as a draw whose scissor covers no pixel shows on Direct3D 12, whose debug layer warns about it.
TEST_P(Direct3DDeviceTest, ReportsTheWarningsOfTheDebugLayerAsErrors) {
    if (GetParam() != Device::Api::Direct3D12) {
        GTEST_SKIP() << "Only the debug layer of Direct3D 12 warns about a scissor without pixels.";
    }
    start();
    if (std::ranges::any_of(warnings, [](const std::string& line) { return line.find("without its debug layer") != std::string::npos; })) {
        GTEST_SKIP() << "Windows lacks the Graphics Tools that bring the debug layer.";
    }
    images.push_back(device->makeImage({.width = kSize, .height = kSize, .kind = Device::ImageKind::RenderTarget, .label = "haylen-test-target"}));
    const Pipeline pipeline = makePipeline(false);
    const Buffer vertices = makeTriangle({-1.0F, -1.0F, -1.0F, 1.0F, 1.0F, -1.0F});

    const Device::ColorAttachment color{.image = images.back()};
    device->beginPass({.colors = std::span(&color, 1)});
    device->applyScissor({0.0F, 0.0F, 0.0F, 0.0F});
    drawTriangle(pipeline, vertices, {1.0F, 0.0F, 0.0F, 1.0F});
    device->endPass();
    device->commit();

    EXPECT_TRUE(std::ranges::any_of(errors, [](const std::string& line) { return line.find("The debug layer of Direct3D 12 warns") != std::string::npos; }));
    errors.clear();
}

TEST_P(Direct3DDeviceTest, ReportsTheErrorsOfAShaderThatDoesNotCompile) {
    start();
    const Device::ProgramDesc desc{.vertex = {.source = kVertexSource, .entry = "main", .profile = "vs_5_0"}, .fragment = {.source = "float4 main() : SV_Target0 { return missing; }", .entry = "main", .profile = "ps_5_0"}, .label = "haylen-test-broken"};
    try {
        static_cast<void>(device->makeProgram(desc));
        FAIL() << "The broken shader compiled.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("The shader \"haylen-test-broken\" does not compile for Direct3D"), std::string::npos) << error.what();
        EXPECT_NE(std::string(error.what()).find("missing"), std::string::npos) << error.what();
    }
}

TEST_P(Direct3DDeviceTest, RepeatsAFrameWithoutAllocatingAndMeasuresTheGpu) {
    start();
    images.push_back(device->makeImage({.width = kSize, .height = kSize, .kind = Device::ImageKind::RenderTarget, .label = "haylen-test-target"}));
    const Image target = images.back();
    images.push_back(device->makeImage({.width = 4, .height = 4, .kind = Device::ImageKind::Dynamic, .label = "haylen-test-dynamic"}));
    const Image dynamic = images.back();
    buffers.push_back(device->makeBuffer({.size = 64, .usage = Device::Usage::Dynamic, .label = "haylen-test-dynamic"}));
    const Buffer vertices = buffers.back();
    const Pipeline pipeline = makePipeline(false);
    const std::array<float, 6> corners{-1.0F, -1.0F, -1.0F, 3.0F, 3.0F, -1.0F};
    const std::array<std::uint8_t, 64> pixels{};

    // clang-format off
    const auto frame = [&] {
        device->writeBuffer(vertices, 0, std::as_bytes(std::span(corners)));
        const Device::ImageWrite write{.image = dynamic, .pixels = std::as_bytes(std::span(pixels))};
        device->writeImages(std::span(&write, 1));
        const Device::ColorAttachment color{.image = target};
        device->beginPass({.colors = std::span(&color, 1), .label = "haylen-test-frame"});
        device->pushDebugGroup("haylen-test-draws");
        for (int draw = 0; draw < 50; ++draw) {
            drawTriangle(pipeline, vertices, {1.0F, 1.0F, 1.0F, 1.0F});
        }
        device->popDebugGroup();
        device->endPass();
        device->commit();
    };
    // clang-format on
    for (int warm = 0; warm < 10; ++warm) {
        frame();
    }
    {
        const test::AllocationTracker tracker;
        for (int index = 0; index < 20; ++index) {
            frame();
        }
        EXPECT_EQ(tracker.getTotal(), 0U);
    }
    EXPECT_TRUE(device->getGpuFrameSeconds().has_value());
}

INSTANTIATE_TEST_SUITE_P(Direct3D, Direct3DDeviceTest, ::testing::ValuesIn(Direct3DDeviceTest::getCompiledApis()), [](const ::testing::TestParamInfo<Device::Api>& info) { return info.param == Device::Api::Direct3D11 ? std::string("Direct3D11") : std::string("Direct3D12"); });

} // namespace haylen::gpu::direct3d
