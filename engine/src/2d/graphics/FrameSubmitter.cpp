#include "2d/graphics/FrameSubmitter.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numeric>
#include <optional>

#include "2d/graphics/MaterialResource.hpp"
#include "2d/graphics/RendererState.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/RenderTargetResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"

namespace haylen::graphics2d {

const PostProcess FrameSubmitter::kNoPostProcess{};

template <typename Block> void FrameSubmitter::applyBlock(int slot, const Block& block) {
    static_assert(sizeof(VertexParams) == RendererState::kVertexBlockSize && sizeof(LitParams) == RendererState::kLitBlockSize && sizeof(LightParams) == 64 && sizeof(CompositeParams) == 176 && sizeof(FilterParams) == 32);
    static_assert(sizeof(BlendVertexParams) == 112 && sizeof(BlendFragmentParams) == 80 && sizeof(BackdropVertexParams) == 96 && sizeof(BackdropFragmentParams) == 32 && sizeof(MetaballVertexParams) == 96 && sizeof(MetaballFragmentParams) == 48);
    state.getGpu().applyUniforms(slot, std::as_bytes(std::span(&block, 1)));
}

FrameSubmitter::Matrix FrameSubmitter::translated(Matrix matrix, math::Vec2 offset) noexcept {
    for (std::size_t row = 0; row < 4; ++row) {
        matrix[12 + row] += matrix[row] * offset.x + matrix[4 + row] * offset.y;
    }
    return matrix;
}

FrameSubmitter::Matrix FrameSubmitter::projection(const math::Transform2D& view, math::Vec2 viewSize) noexcept {
    const float scaleX = 2.0F / viewSize.x;
    const float scaleY = -2.0F / viewSize.y;
    Matrix matrix{};
    matrix[0] = view.a * scaleX;
    matrix[1] = view.b * scaleY;
    matrix[4] = view.c * scaleX;
    matrix[5] = view.d * scaleY;
    matrix[10] = 1.0F;
    matrix[12] = view.tx * scaleX - 1.0F;
    matrix[13] = view.ty * scaleY + 1.0F;
    matrix[15] = 1.0F;
    return matrix;
}

bool FrameSubmitter::isInstanced(const DrawItem& item) noexcept {
    return (item.program == Program::Sprite || item.program == Program::Text || item.program == Program::Recolor || item.program == Program::Effect || item.program == Program::Shape || item.program == Program::Batch) && item.batch == nullptr;
}

void FrameSubmitter::submit() {
    prepareTargets();
    castShadows();
    buildCommands();
    upload();

    // A draw can still fail inside a pass, such as a material whose program the backend rejects, so the pass and the frame end before the error goes on and the next frame starts clean.
    try {
        renderOffscreen();
        renderSwapchain();
    } catch (...) {
        if (passOpen) {
            endPass();
        }
        state.getGpu().commit();
        throw;
    }
}

void FrameSubmitter::prepareTargets() {
    std::size_t litCount = 0;
    std::size_t fieldCount = 0;
    for (Canvas& canvas : state.canvases) {
        const math::Vec2 size = state.getPassRect(canvas).getSize();
        if (canvas.isComposited()) {
            canvas.litIndex = litCount++;
            getLitTargets(canvas.litIndex, size, canvas);
        }
        if (canvas.hasPostMaterials()) {
            canvas.postShade = state.shades.size();
            for (const Material& material : canvas.options.postProcess->materials) {
                state.addMaterialShade(material);
            }
        }
        for (std::size_t index = canvas.metaballBegin; index < canvas.metaballEnd; ++index) {
            state.metaballs[index].field = fieldCount;
            getField(fieldCount++, size);
        }
    }

    // Targets past what this frame uses go back to the device, so a peak such as a transition between two lit scenes does not keep its images.
    state.litTargets.resize(litCount);
    state.fields.resize(fieldCount);
}

LitTargets& FrameSubmitter::getLitTargets(std::size_t index, math::Vec2 size, const Canvas& canvas) {
    const int width = std::max(1, static_cast<int>(std::lround(size.x)));
    const int height = std::max(1, static_cast<int>(std::lround(size.y)));
    if (state.litTargets.size() <= index) {
        state.litTargets.resize(index + 1);
    }

    // A new size starts over, and the targets a canvas needs appear the first time it needs them.
    LitTargets& targets = state.litTargets[index];
    if (!targets.scene.isValid() || targets.scene.getWidth() != width || targets.scene.getHeight() != height) {
        targets = {};
        targets.scene = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
    }
    if (canvas.isLit() && !targets.light.isValid()) {
        targets.emission = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.surface = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.info = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.light = state.device.getState().createRenderTarget(width, height, state.lightFormat, {.filter = graphics::Texture::Filter::Linear});
    }
    if ((canvas.hasPostMaterials() || canvas.isStaged()) && !targets.post[0].isValid()) {
        for (graphics::RenderTarget& image : targets.post) {
            image = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
        }
    }
    if (canvas.distorted && !targets.distortion.isValid()) {
        targets.distortion = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
    }
    if (canvas.backdrop && !targets.backdrop.isValid()) {
        targets.backdrop = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
    }

    // Bloom and blur work at half the size, which their filters read with linear filtering.
    const int halfWidth = std::max(1, width / 2);
    const int halfHeight = std::max(1, height / 2);
    for (auto [used, pair] : {std::pair{canvas.hasBloom(), &targets.bloom}, std::pair{canvas.hasBlur(), &targets.blur}}) {
        if (used && !(*pair)[0].isValid()) {
            for (graphics::RenderTarget& image : *pair) {
                image = state.device.createRenderTarget(halfWidth, halfHeight, {.filter = graphics::Texture::Filter::Linear});
            }
        }
    }
    return targets;
}

// Metaball fields have half the resolution of their canvas, and linear filtering keeps their surface smooth.
const graphics::RenderTarget& FrameSubmitter::getField(std::size_t index, math::Vec2 size) {
    const int width = std::max(1, static_cast<int>(std::lround(size.x * 0.5F)));
    const int height = std::max(1, static_cast<int>(std::lround(size.y * 0.5F)));
    if (state.fields.size() <= index) {
        state.fields.resize(index + 1);
    }

    graphics::RenderTarget& field = state.fields[index];
    if (!field.isValid() || field.getWidth() != width || field.getHeight() != height) {
        field = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
    }
    return field;
}

// Every shadowed light takes one row of the atlas, and the rows are cast in parallel from the occluders of their canvas.
void FrameSubmitter::castShadows() {
    if (state.lights.empty()) {
        return;
    }

    std::vector<std::pair<LightDraw*, const Canvas*>>& casts = state.shadowCasts;
    for (const Canvas& canvas : state.canvases) {
        for (std::size_t index = canvas.lightBegin; index < canvas.lightEnd; ++index) {
            LightDraw& draw = state.lights[index];
            if (draw.light.shadows) {
                draw.shadowRow = static_cast<int>(casts.size());
                casts.emplace_back(&draw, &canvas);
            }
        }
    }

    const std::size_t rows = std::max<std::size_t>(casts.size(), 1);
    if (state.shadowRows < static_cast<int>(rows)) {
        state.shadowRows = static_cast<int>(std::bit_ceil(static_cast<unsigned>(std::max<std::size_t>(rows, 8))));
        if (state.shadowImage.isValid()) {
            state.device.getState().graveyard->bury(state.shadowImage);
        }
        state.shadowImage = state.getGpu().makeImage({.width = lighting2d::ShadowMap::kResolution, .height = state.shadowRows, .format = gpu::Device::PixelFormat::R32Float, .kind = gpu::Device::ImageKind::Dynamic, .label = "haylen-shadow-atlas"});
    }
    state.shadowTexels.assign(static_cast<std::size_t>(state.shadowRows) * lighting2d::ShadowMap::kResolution, lighting2d::ShadowMap::kClear);
    // clang-format off
    state.jobs.parallelFor(0, casts.size(), 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            const auto& [draw, canvas] = casts[index];
            const std::span<float> row(state.shadowTexels.data() + index * lighting2d::ShadowMap::kResolution, lighting2d::ShadowMap::kResolution);
            const std::span<const lighting2d::ShadowMap::Segment> segments(state.segments.data() + canvas->segmentBegin, canvas->segmentEnd - canvas->segmentBegin);
            draw->axis = lighting2d::ShadowMap::cast(draw->light, segments, canvas->getWorldBounds(), row);
        }
    });
    // clang-format on

    // The atlas goes to the GPU with the dynamic textures of the frame.
    shadowWrite = gpu::Device::ImageWrite{.image = state.shadowImage, .pixels = std::as_bytes(std::span(state.shadowTexels))};
    state.stats.uploadedBytes += shadowWrite->pixels.size();
    state.stats.shadows = casts.size();
}

// Orders every canvas, groups compatible neighbours into commands and lays instance data out in draw order.
void FrameSubmitter::buildCommands() {
    std::vector<std::uint32_t>& order = state.itemOrder;
    std::uint32_t expected = 0;
    bool inOrder = true;

    for (Canvas& canvas : state.canvases) {
        order.resize(canvas.itemEnd - canvas.itemBegin);
        std::iota(order.begin(), order.end(), static_cast<std::uint32_t>(canvas.itemBegin));
        // clang-format off
        std::sort(order.begin(), order.end(), [&](std::uint32_t lhs, std::uint32_t rhs) {
            const DrawItem& a = state.items[lhs];
            const DrawItem& b = state.items[rhs];
            if (a.key != b.key) {
                return a.key < b.key;
            }
            return a.sortId != b.sortId ? a.sortId < b.sortId : a.sequence < b.sequence;
        });
        // clang-format on

        canvas.sceneBegin = state.commands.size();
        appendCanvas(order, false, expected, inOrder);
        canvas.sceneEnd = state.commands.size();
        canvas.distortBegin = canvas.sceneEnd;
        if (canvas.distorted) {
            appendCanvas(order, true, expected, inOrder);
        }
        canvas.distortEnd = state.commands.size();
    }

    identity = inOrder && expected == state.instances.size();
}

// The draws of the image of a canvas and its distortion draws become separate runs of commands, which never merge across.
void FrameSubmitter::appendCanvas(const std::vector<std::uint32_t>& order, bool distortion, std::uint32_t& expected, bool& inOrder) {
    commandFloor = state.commands.size();
    for (const std::uint32_t index : order) {
        const DrawItem& item = state.items[index];
        if (item.distortion != distortion) {
            continue;
        }
        if (isInstanced(item)) {
            inOrder = inOrder && item.first == expected;
            expected = item.first + item.count;
        }
        append(item);
    }
}

void FrameSubmitter::append(const DrawItem& item) {
    Command command{.program = item.program, .blend = item.blend, .clip = item.clip, .shade = item.shade, .texture = item.texture, .batch = item.batch, .offset = item.offset, .count = item.count};

    if (item.program == Program::Mesh) {
        command.first = static_cast<std::uint32_t>(state.uploadIndices.size());
        const auto first = state.indices.begin() + static_cast<std::ptrdiff_t>(item.first);
        state.uploadIndices.insert(state.uploadIndices.end(), first, first + static_cast<std::ptrdiff_t>(item.count));
    } else if (item.program == Program::ImageBlend || item.program == Program::Backdrop || item.program == Program::Metaball) {
        command.first = item.first;
    } else if (item.batch == nullptr) {
        command.first = static_cast<std::uint32_t>(instanceCursor);
        state.placements.emplace_back(item.first, item.count);
        instanceCursor += item.count;
    }

    // Neighbours that share state and continue each other's data become one draw call.
    if (state.commands.size() > commandFloor) {
        Command& previous = state.commands.back();
        const bool sameShade = previous.shade == command.shade || state.shades[previous.shade].matches(state.shades[command.shade]);
        const bool sameState = previous.program == command.program && previous.blend == command.blend && previous.clip == command.clip && sameShade && previous.texture == command.texture;
        const bool single = command.program == Program::ImageBlend || command.program == Program::Backdrop || command.program == Program::Metaball;
        const bool mergeable = !single && previous.batch == nullptr && command.batch == nullptr && previous.first + previous.count == command.first;
        if (sameState && mergeable) {
            previous.count += command.count;
            return;
        }
    }
    state.commands.push_back(command);
}

// The splats of metaball fields follow the instances of the draws in the same buffer.
void FrameSubmitter::upload() {
    state.vectors.stage();
    state.stats.uploadedBytes += state.device.uploadTextures(shadowWrite ? std::span(&*shadowWrite, 1) : std::span<const gpu::Device::ImageWrite>());

    const std::vector<GpuInstance>* source = &state.instances;
    if (!identity) {
        state.upload.resize(instanceCursor);
        std::size_t cursor = 0;
        for (const auto& [first, count] : state.placements) {
            std::copy_n(state.instances.begin() + static_cast<std::ptrdiff_t>(first), count, state.upload.begin() + static_cast<std::ptrdiff_t>(cursor));
            cursor += count;
        }
        source = &state.upload;
    }

    splatBase = source->size();
    const std::size_t total = source->size() + state.splats.size();
    if (total > 0) {
        state.ensureBuffer(state.instanceBuffer, state.instanceCapacity, total, sizeof(GpuInstance), false);
    }
    // clang-format off
    const auto write = [&](std::span<const GpuInstance> data, std::size_t offset) {
        if (data.empty()) {
            return;
        }
        state.getGpu().writeBuffer(state.instanceBuffer, offset * sizeof(GpuInstance), std::as_bytes(data));
        state.stats.uploadedBytes += data.size_bytes();
    };
    // clang-format on
    write(*source, 0);
    write(state.splats, splatBase);
    writeBuffer(state.vertexBuffer, state.vertexCapacity, std::as_bytes(std::span(state.vertices)), sizeof(GpuVertex), false);
    writeBuffer(state.indexBuffer, state.indexCapacity, std::as_bytes(std::span(state.uploadIndices)), sizeof(std::uint32_t), true);

    // Every light takes the slot of its index, so the canvases draw their own ranges of the buffer.
    state.lightInstances.resize(state.lights.size());
    for (const Canvas& canvas : state.canvases) {
        const math::Rect bounds = canvas.getWorldBounds();
        for (std::size_t index = canvas.lightBegin; index < canvas.lightEnd; ++index) {
            state.lightInstances[index] = LightInstance::make(state.lights[index], bounds);
        }
    }
    writeBuffer(state.lightBuffer, state.lightCapacity, std::as_bytes(std::span(state.lightInstances)), sizeof(LightInstance), false);

    state.stats.instances = source->size();
    state.stats.vertices = state.vertices.size();
    state.stats.indices = state.uploadIndices.size();
}

void FrameSubmitter::writeBuffer(gpu::Device::Buffer& buffer, std::size_t& capacity, std::span<const std::byte> data, std::size_t elementSize, bool indexBuffer) {
    if (data.empty()) {
        return;
    }
    state.ensureBuffer(buffer, capacity, data.size() / elementSize, elementSize, indexBuffer);
    state.getGpu().writeBuffer(buffer, 0, data);
    state.stats.uploadedBytes += data.size();
}

// Passes run in the order the frame recorded them, so a capture renders once the canvases it holds have their offscreen passes, and before any later canvas that draws its texture. Captures render in the order they closed, which puts a capture inside another one before the outer one.
void FrameSubmitter::renderOffscreen() {
    const std::vector<std::size_t>& captures = state.closedCaptures;
    std::size_t nextCapture = 0;
    for (std::size_t index = 0; index < state.canvases.size(); ++index) {
        while (nextCapture < captures.size() && state.captures[captures[nextCapture]].canvasEnd <= index) {
            renderCapture(captures[nextCapture++]);
        }
        renderFields(state.canvases[index]);
        renderCanvasOffscreen(state.canvases[index]);
    }
    while (nextCapture < captures.size()) {
        renderCapture(captures[nextCapture++]);
    }
}

// Every point of a metaball draw adds its soft circle to the field, which covers the view of the canvas.
void FrameSubmitter::renderFields(const Canvas& canvas) {
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    const graphics::TextureResource& kernel = *state.metaball.getResource();
    for (std::size_t index = canvas.metaballBegin; index < canvas.metaballEnd; ++index) {
        const MetaballDraw& draw = state.metaballs[index];
        beginOffscreenPass(state.fields[draw.field], math::Color::transparent());
        state.getGpu().applyPipeline(state.getPipeline(Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Additive), graphics::PassTarget::Offscreen));
        applyUniforms(matrix, graphics::BlendMode::Type::Additive);

        gpu::Device::Bindings bindings;
        bindings.vertexBuffers[0] = state.quad;
        bindings.vertexBuffers[1] = state.instanceBuffer;
        bindings.vertexOffsets[1] = static_cast<int>((splatBase + draw.first) * sizeof(GpuInstance));
        bindings.textures[RendererState::kSpriteTexture] = kernel.image;
        bindings.samplers[RendererState::kSpriteSampler] = kernel.sampler;
        state.getGpu().applyBindings(bindings);
        state.getGpu().draw(0, 4, static_cast<int>(draw.count));
        ++state.stats.drawCalls;
        endPass();
    }
}

void FrameSubmitter::renderCanvasOffscreen(Canvas& canvas) {
    if (!canvas.isComposited()) {
        if (canvas.kind == Canvas::Kind::Target) {
            const math::Rect rect = state.getPassRect(canvas);
            beginOffscreenPass(canvas.target, canvas.options.clear.value_or(math::Color::transparent()).getPremultiplied());
            state.getGpu().applyViewport(rect);
            drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, rect);
            endPass();
        }
        return;
    }

    const LitTargets& targets = state.litTargets[canvas.litIndex];
    const math::Rect full{0.0F, 0.0F, static_cast<float>(targets.scene.getWidth()), static_cast<float>(targets.scene.getHeight())};
    const math::Color clear = canvas.options.clear.value_or(canvas.kind == Canvas::Kind::Target ? math::Color::transparent() : state.clearColor).getPremultiplied();
    if (canvas.distortEnd > canvas.distortBegin) {
        beginOffscreenPass(targets.distortion, math::Color::transparent());
        drawCommands(canvas, canvas.distortBegin, canvas.distortEnd, graphics::PassTarget::Offscreen, full);
        endPass();
    }
    if (canvas.isLit()) {
        beginLitPass(targets, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::LitScene, full);
        endPass();
        renderLights(canvas, targets);
    } else {
        beginOffscreenPass(targets.scene, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, full);
        endPass();
    }
    renderPostChain(canvas, targets);

    // A render target canvas takes its finished image the way a world canvas reaches the screen.
    if (canvas.kind == Canvas::Kind::Target) {
        const math::Rect rect = state.getPassRect(canvas);
        beginOffscreenPass(canvas.target, clear);
        state.getGpu().applyViewport(rect);
        drawFinal(canvas, graphics::PassTarget::Offscreen);
        endPass();
    }
}

// Neighbouring lights that share a blend mode and a shape draw as the instances of one call, in the order the app drew them.
void FrameSubmitter::renderLights(const Canvas& canvas, const LitTargets& targets) {
    beginOffscreenPass(targets.light, *canvas.options.ambientLight);
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    const LightParams vertex{.viewProjection = matrix};
    const gpu::Device::Sampler nearest = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});

    std::size_t first = canvas.lightBegin;
    while (first < canvas.lightEnd) {
        const lighting2d::Light& light = state.lights[first].light;
        const graphics::TextureResource& shape = getLightShape(light);
        std::size_t end = first + 1;
        while (end < canvas.lightEnd && state.lights[end].light.blend == light.blend && &getLightShape(state.lights[end].light) == &shape) {
            ++end;
        }

        state.getGpu().applyPipeline(state.getPipeline(Program::Light, static_cast<std::uint8_t>(light.blend), graphics::PassTarget::LightMap));
        gpu::Device::Bindings bindings;
        bindings.vertexBuffers[0] = state.quad;
        bindings.vertexBuffers[1] = state.lightBuffer;
        bindings.vertexOffsets[1] = static_cast<int>(first * sizeof(LightInstance));
        bindings.textures[RendererState::kLightShape] = shape.image;
        bindings.textures[RendererState::kLightSurface] = targets.surface.getTexture().getResource()->image;
        bindings.textures[RendererState::kLightInfo] = targets.info.getTexture().getResource()->image;
        bindings.textures[RendererState::kLightShadow] = state.shadowImage;
        bindings.samplers[RendererState::kLightShapeSampler] = shape.sampler;
        bindings.samplers[RendererState::kLightSurfaceSampler] = nearest;
        bindings.samplers[RendererState::kLightShadowSampler] = nearest;
        state.getGpu().applyBindings(bindings);
        applyBlock(RendererState::kVertexBlock, vertex);
        state.getGpu().draw(0, 4, static_cast<int>(end - first));
        ++state.stats.drawCalls;
        first = end;
    }
    endPass();
}

const graphics::TextureResource& FrameSubmitter::getLightShape(const lighting2d::Light& light) const {
    return light.texture.isValid() ? *light.texture.getResource() : *state.light.getResource();
}

// A staged canvas composites its first stage into the first post target, where bloom and blur read it, and finishes the image in the destination, or in the other post target when materials follow. The composite of a canvas with materials writes into a post target, and every material but the last draws the image before it into the other one, which leaves the input of the last material in `postImage`.
void FrameSubmitter::renderPostChain(Canvas& canvas, const LitTargets& targets) {
    const bool staged = canvas.isStaged();
    if (!staged && !canvas.hasPostMaterials()) {
        return;
    }

    std::size_t current = 0;
    if (staged) {
        const PostProcess& post = *canvas.options.postProcess;
        beginOffscreenPass(targets.post[0], math::Color::transparent());
        composite(canvas, graphics::PassTarget::Offscreen, Stage::First);
        endPass();
        if (canvas.hasBloom()) {
            blurInto(canvas, targets.post[0], targets.bloom, post.bloomThreshold, post.bloomRadius);
        }
        if (canvas.hasBlur()) {
            blurInto(canvas, targets.post[0], targets.blur, -1.0F, post.blur);
        }
        if (!canvas.hasPostMaterials()) {
            return;
        }
        beginOffscreenPass(targets.post[1], math::Color::transparent());
        composite(canvas, graphics::PassTarget::Offscreen, Stage::Finish);
        endPass();
        current = 1;
    } else {
        beginOffscreenPass(targets.post[0], math::Color::transparent());
        composite(canvas, graphics::PassTarget::Offscreen, Stage::Whole);
        endPass();
    }

    const std::size_t count = canvas.options.postProcess->materials.size();
    for (std::size_t index = 0; index + 1 < count; ++index) {
        beginOffscreenPass(targets.post[1 - current], math::Color::transparent());
        drawPostMaterial(canvas.postShade + index, targets.post[current].getTexture(), graphics::PassTarget::Offscreen);
        endPass();
        current = 1 - current;
    }
    canvas.postImage = current;
}

void FrameSubmitter::renderCapture(std::size_t index) {
    const Capture& capture = state.captures[index];
    beginOffscreenPass(capture.target, capture.clear.getPremultiplied());
    drawCanvases(index + 1, graphics::PassTarget::Offscreen);
    endPass();
}

// The frame of an opaque window clears its alpha to 1 and never writes it, whatever the clear color and the blends of the app, and a transparent window holds premultiplied colors.
void FrameSubmitter::renderSwapchain() {
    const math::Color clear = transparent ? state.clearColor.getPremultiplied() : state.clearColor.withAlpha(1.0F);
    state.getGpu().beginPass({.load = gpu::Device::Load::Clear, .clear = clear, .label = "haylen-swapchain-pass"});
    passOpen = true;
    ++state.stats.passes;

    drawCanvases(0, transparent ? graphics::PassTarget::TransparentSwapchain : graphics::PassTarget::Swapchain);
    endPass();
    state.getGpu().commit();
}

// Draws the world and screen canvases of a destination, the screen or a capture, by their order and then in the order they began, which is their place in the list of canvases. A stable sort would take a buffer of its own in some standard libraries.
void FrameSubmitter::drawCanvases(std::size_t capture, graphics::PassTarget pass) {
    std::vector<const Canvas*>& ordered = state.orderedCanvases;
    ordered.clear();
    for (const Canvas& canvas : state.canvases) {
        if (canvas.kind != Canvas::Kind::Target && canvas.capture == capture) {
            ordered.push_back(&canvas);
        }
    }
    std::ranges::sort(ordered, [](const Canvas* lhs, const Canvas* rhs) { return lhs->options.order != rhs->options.order ? lhs->options.order < rhs->options.order : lhs < rhs; });

    for (const Canvas* canvas : ordered) {
        const math::Rect rect = state.getPassRect(*canvas);
        state.getGpu().applyViewport(rect);
        if (canvas->isComposited()) {
            // The clip of an earlier canvas stays applied until something sets another one.
            state.getGpu().applyScissor(rect);
            drawFinal(*canvas, pass);
            continue;
        }
        drawCommands(*canvas, canvas->sceneBegin, canvas->sceneEnd, pass, rect);
    }
}

void FrameSubmitter::drawFinal(const Canvas& canvas, graphics::PassTarget pass) {
    if (!canvas.hasPostMaterials()) {
        composite(canvas, pass, canvas.isStaged() ? Stage::Finish : Stage::Whole);
        return;
    }
    const LitTargets& targets = state.litTargets[canvas.litIndex];
    drawPostMaterial(canvas.postShade + canvas.options.postProcess->materials.size() - 1, targets.post[canvas.postImage].getTexture(), pass);
}

// A pass without a clear color keeps what the target holds, such as a canvas that goes on after a group read what lies below it.
void FrameSubmitter::beginOffscreenPass(const graphics::RenderTarget& renderTarget, std::optional<math::Color> clear) {
    const gpu::Device::ColorAttachment attachment{.image = renderTarget.getTexture().getResource()->image, .load = clear ? gpu::Device::Load::Clear : gpu::Device::Load::Keep, .clear = clear.value_or(math::Color::transparent())};
    state.getGpu().beginPass({.colors = std::span(&attachment, 1), .label = "haylen-offscreen-pass"});
    passOpen = true;
    ++state.stats.passes;
}

// The info image starts with the light mask 1 and the layer 0 of draws that set neither, so lights reach the background like such a draw.
void FrameSubmitter::beginLitPass(const LitTargets& targets, math::Color clear) {
    const std::array<gpu::Device::ColorAttachment, 4> attachments{{
        {.image = targets.scene.getTexture().getResource()->image, .clear = clear},
        {.image = targets.emission.getTexture().getResource()->image, .clear = math::Color(0.0F, 0.0F, 0.0F, 0.0F)},
        {.image = targets.surface.getTexture().getResource()->image, .clear = math::Color(0.5F, 0.5F, 0.0F, 0.0F)},
        {.image = targets.info.getTexture().getResource()->image, .clear = math::Color(1.0F / 255.0F, 128.0F / 255.0F, 0.0F, 0.0F)},
    }};
    state.getGpu().beginPass({.colors = attachments, .label = "haylen-lit-scene-pass"});
    passOpen = true;
    ++state.stats.passes;
}

void FrameSubmitter::endPass() {
    state.getGpu().endPass();
    passOpen = false;
}

// The first stage reads the scene, the light, the emission and the distortion map, and the finish reads the image of the first stage, or its blur, and the bloom. The textures a stage leaves out bind the scene, which the shader never reads.
void FrameSubmitter::composite(const Canvas& canvas, graphics::PassTarget pass, Stage stage) {
    const LitTargets& targets = state.litTargets[canvas.litIndex];
    const PostProcess& post = canvas.options.postProcess ? *canvas.options.postProcess : kNoPostProcess;
    const bool lit = canvas.isLit() && stage != Stage::Finish;
    const bool distorts = canvas.distortEnd > canvas.distortBegin && stage != Stage::Finish;
    const bool blooms = canvas.hasBloom() && stage != Stage::First;
    const bool grades = post.colorLut.isValid() && stage != Stage::First;
    const graphics::RenderTarget& image = stage != Stage::Finish ? targets.scene : (canvas.hasBlur() ? targets.blur[0] : targets.post[0]);
    const auto viewOf = [](const graphics::RenderTarget& used) { return used.getTexture().getResource()->image; };

    state.getGpu().applyPipeline(state.getPipeline(Program::Composite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Premultiplied), pass));
    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kCompositeScene] = viewOf(image);
    bindings.textures[RendererState::kCompositeLight] = viewOf(lit ? targets.light : image);
    bindings.textures[RendererState::kCompositeEmission] = viewOf(lit ? targets.emission : image);
    bindings.textures[RendererState::kCompositeDistortion] = viewOf(distorts ? targets.distortion : image);
    bindings.textures[RendererState::kCompositeBloom] = viewOf(blooms ? targets.bloom[0] : image);
    bindings.textures[RendererState::kCompositeLut] = grades ? post.colorLut.getResource()->image : viewOf(image);
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Linear});
    state.getGpu().applyBindings(bindings);

    const float stageFlag = stage == Stage::Whole ? 0.0F : (stage == Stage::First ? 1.0F : 2.0F);
    const std::array<std::array<float, 3>, 3> vision = post.getColorVisionMatrix();
    const CompositeParams params{
        .tint = {post.tint.r, post.tint.g, post.tint.b, post.tint.a},
        .fade = {post.fade.r, post.fade.g, post.fade.b, post.fade.a},
        .grading = {post.saturation, post.brightness, post.contrast, grades ? post.colorLutStrength : 0.0F},
        .vignette = {post.vignetteStrength, post.vignetteRadius, post.vignetteSoftness, blooms ? post.bloomStrength : 0.0F},
        .flags = {lit ? 1.0F : 0.0F, state.getGpu().isOriginTopLeft() ? 0.0F : 1.0F, stageFlag, distorts ? 1.0F : 0.0F},
        .view = {canvas.viewSize.x, canvas.viewSize.y, post.distortion, post.chromaticAberration},
        .shape = {post.pixelate, grades ? post.colorLut.getSize().y : 0.0F, 0.0F, 0.0F},
        .visionRed = {vision[0][0], vision[0][1], vision[0][2], 0.0F},
        .visionGreen = {vision[1][0], vision[1][1], vision[1][2], 0.0F},
        .visionBlue = {vision[2][0], vision[2][1], vision[2][2], 0.0F},
        .grain = {post.grain, post.dither ? 1.0F : 0.0F, static_cast<float>(state.epoch % kGrainFrames), 0.0F},
    };
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::filter(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, float threshold, math::Vec2 step) {
    const math::Vec2 size = source.getSize();
    beginOffscreenPass(destination, math::Color::transparent());
    state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen));
    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kFilterSource] = source.getTexture().getResource()->image;
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Linear});
    state.getGpu().applyBindings(bindings);

    const bool shrinks = step.isZero();
    const FilterParams params{
        .settings = {shrinks ? 0.0F : 1.0F, threshold, state.getGpu().isOriginTopLeft() ? 0.0F : 1.0F, 0.0F},
        .offset = {step.x / size.x, step.y / size.y, 0.5F / size.x, 0.5F / size.y},
    };
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
    endPass();
}

void FrameSubmitter::copy(const graphics::RenderTarget& source, const graphics::RenderTarget& destination) {
    beginOffscreenPass(destination, math::Color::transparent());
    state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen));
    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kFilterSource] = source.getTexture().getResource()->image;
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});
    state.getGpu().applyBindings(bindings);
    const FilterParams params{.settings = {2.0F, 0.0F, state.getGpu().isOriginTopLeft() ? 0.0F : 1.0F, 0.0F}, .offset = {}};
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
    endPass();
}

// Nine taps of the blur span four steps on each side, so the steps of a radius are a quarter of it in texels of the half-size image.
void FrameSubmitter::blurInto(const Canvas& canvas, const graphics::RenderTarget& image, const std::array<graphics::RenderTarget, 2>& pair, float threshold, float radius) {
    filter(image, pair[0], threshold, {});
    const float texelsPerUnit = pair[0].getSize().x / std::max(canvas.viewSize.x, 1.0F);
    const float spacing = std::max(radius * texelsPerUnit * 0.25F, 0.5F);
    filter(pair[0], pair[1], -1.0F, {spacing, 0.0F});
    filter(pair[1], pair[0], -1.0F, {0.0F, spacing});
}

// A post-processing material draws the image as one sprite over the unit square, which covers the viewport of the pass.
void FrameSubmitter::drawPostMaterial(std::size_t shadeIndex, const graphics::Texture& image, graphics::PassTarget pass) {
    const Shade& shade = state.shades[shadeIndex];
    const graphics::TextureResource& resource = *image.getResource();
    state.getGpu().applyPipeline(state.getMaterialPipeline(*shade.material, Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Premultiplied), pass));
    applyUniforms(projection(math::Transform2D::identity(), {1.0F, 1.0F}), graphics::BlendMode::Type::Premultiplied);
    applyShade(shade, pass, Program::Sprite, graphics::BlendMode::Type::Premultiplied);

    gpu::Device::Bindings bindings;
    bindings.vertexBuffers[0] = state.quad;
    bindings.vertexBuffers[1] = state.screen;
    bindings.vertexOffsets[1] = resource.flipped ? static_cast<int>(sizeof(GpuInstance)) : 0;
    bindMaterial(bindings, shade, resource, pass, Program::Sprite);
    state.getGpu().applyBindings(bindings);
    state.getGpu().draw(0, 4, 1);
    ++state.stats.drawCalls;
}

// A pixel belongs to a clip when its center does, so each edge of the clip rounds to the nearest pixel boundary on its own.
math::Rect FrameSubmitter::getScissor(const Canvas& canvas, const math::Rect& clip, const math::Rect& passRect) noexcept {
    const math::Vec2 scale = passRect.getSize() / canvas.viewSize;
    const std::array<math::Vec2, 4> corners{canvas.view.apply(clip.getMin()), canvas.view.apply(math::Vec2{clip.getRight(), clip.getTop()}), canvas.view.apply(clip.getMax()), canvas.view.apply(math::Vec2{clip.getLeft(), clip.getBottom()})};
    const math::Rect units = math::Geometry::bounds(corners);
    const math::Vec2 first = math::Vec2::round(passRect.getMin() + units.getMin() * scale);
    const math::Vec2 last = math::Vec2::round(passRect.getMin() + units.getMax() * scale);
    const math::Rect scissor = math::Rect::fromMinMax(first, last).intersection(passRect);
    return {scissor.x, scissor.y, std::max(0.0F, scissor.width), std::max(0.0F, scissor.height)};
}

void FrameSubmitter::applyClip(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) {
    const math::Rect scissor = clip == 0 ? passRect : getScissor(canvas, state.clips[clip - 1U], passRect);
    state.getGpu().applyScissor(scissor);
}

void FrameSubmitter::drawCommands(const Canvas& canvas, std::size_t begin, std::size_t end, graphics::PassTarget pass, const math::Rect& passRect) {
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    gpu::Device::Pipeline currentPipeline;
    graphics::TextureResource* currentTexture = nullptr;
    std::optional<std::uint32_t> currentClip;
    std::uint32_t currentShade = 0;
    math::Vec2 currentOffset{};

    for (std::size_t index = begin; index < end; ++index) {
        const Command& command = state.commands[index];
        const Shade& shade = state.shades[command.shade];
        if (command.program == Program::Backdrop) {
            drawBackdrop(canvas, command, matrix, passRect);
            currentPipeline = {};
            currentClip.reset();
            continue;
        }
        if (command.program == Program::ImageBlend || command.program == Program::Metaball) {
            // These apply their own pipeline, uniforms and clip, so the next command applies them again.
            state.getGpu().applyPipeline(state.getPipeline(command.program, static_cast<std::uint8_t>(command.blend), pass));
            applyClip(canvas, command.clip, passRect);
            applyShade(shade, pass, command.program, command.blend);
            if (command.program == Program::ImageBlend) {
                drawImageBlend(command, matrix);
            } else {
                drawMetaball(command, matrix);
            }
            currentPipeline = {};
            currentClip.reset();
            continue;
        }

        const auto blend = static_cast<std::uint8_t>(command.blend);
        const gpu::Device::Pipeline pipeline = shade.material != nullptr ? state.getMaterialPipeline(*shade.material, command.program, blend, pass) : state.getPipeline(command.program, blend, pass);
        const bool fresh = pipeline.id != currentPipeline.id;
        if (fresh) {
            state.getGpu().applyPipeline(pipeline);
            currentPipeline = pipeline;
            currentClip.reset();
        }
        if (fresh || command.offset != currentOffset) {
            applyUniforms(translated(matrix, command.offset), command.blend);
            currentOffset = command.offset;
        }
        if (fresh || command.shade != currentShade) {
            applyShade(shade, pass, command.program, command.blend);
            currentShade = command.shade;
        }
        if (currentClip != command.clip) {
            applyClip(canvas, command.clip, passRect);
            currentClip = command.clip;
        }
        if (currentTexture != command.texture) {
            ++state.stats.textureSwitches;
            currentTexture = command.texture;
        }

        gpu::Device::Bindings bindings;
        if (command.program == Program::Mesh) {
            bindings.vertexBuffers[0] = state.vertexBuffer;
            bindings.indexBuffer = state.indexBuffer;
        } else {
            bindings.vertexBuffers[0] = state.quad;
            bindings.vertexBuffers[1] = command.batch != nullptr ? command.batch->buffer : state.instanceBuffer;
            bindings.vertexOffsets[1] = static_cast<int>(command.first * sizeof(GpuInstance));
        }
        const bool paired = command.program == Program::Recolor || command.program == Program::Effect || command.program == Program::Batch;
        if (paired) {
            bindings.vertexBuffers[2] = state.instanceBuffer;
            bindings.vertexOffsets[2] = static_cast<int>((command.first + 1) * sizeof(GpuInstance));
        }
        if (command.program == Program::Recolor) {
            bindings.textures[RendererState::kPartMask] = shade.partMask->image;
        }
        if (shade.material != nullptr) {
            bindMaterial(bindings, shade, *command.texture, pass, command.program);
        } else if (command.program == Program::Batch) {
            bindBatch(bindings, shade, *command.texture, pass);
        } else {
            bindings.textures[RendererState::kSpriteTexture] = command.texture->image;
            bindings.samplers[RendererState::kSpriteSampler] = command.texture->sampler;
            if (pass == graphics::PassTarget::LitScene && command.program == Program::Sprite) {
                bindings.textures[RendererState::kNormalTexture] = (shade.normalMap != nullptr ? shade.normalMap : state.white.getResource().get())->image;
            }
        }
        state.getGpu().applyBindings(bindings);

        if (command.program == Program::Mesh) {
            state.getGpu().draw(static_cast<int>(command.first), static_cast<int>(command.count), 1);
        } else {
            state.getGpu().draw(0, 4, static_cast<int>(paired ? command.count / 2 : command.count));
        }
        ++state.stats.drawCalls;
    }
}

void FrameSubmitter::drawImageBlend(const Command& command, const Matrix& matrix) {
    const BlendDraw& draw = state.blends[command.first];
    const ImageBlend& blend = draw.blend;
    const graphics::TextureResource& from = *blend.from.getResource();
    const graphics::TextureResource& to = *blend.to.getResource();

    gpu::Device::Bindings bindings;
    bindings.vertexBuffers[0] = state.quad;
    bindings.textures[RendererState::kBlendFrom] = from.image;
    bindings.textures[RendererState::kBlendTo] = to.image;
    bindings.samplers[0] = from.sampler;
    state.getGpu().applyBindings(bindings);

    BlendVertexParams vertex{.viewProjection = matrix};
    vertex.placement[0] = draw.origin.x;
    vertex.placement[1] = draw.origin.y;
    vertex.placement[2] = draw.rows.x;
    vertex.placement[3] = draw.rows.y;
    vertex.columns[0] = draw.columns.x;
    vertex.columns[1] = draw.columns.y;
    vertex.premultiply = RendererState::expectsPremultiplied(command.blend) ? 1.0F : 0.0F;
    applyBlock(RendererState::kVertexBlock, vertex);

    const float flips = (from.flipped ? 1.0F : 0.0F) + (to.flipped ? 2.0F : 0.0F);
    const float rows = draw.rows.getLength();
    const float columns = draw.columns.getLength();
    const BlendFragmentParams fragment{
        .settings = {static_cast<float>(blend.pattern), blend.progress, blend.reversed ? 1.0F : 0.0F, blend.angle},
        .shape = {blend.center.x, blend.center.y, blend.cellSize, blend.blockSize},
        .color = {blend.color.r, blend.color.g, blend.color.b, blend.color.a},
        .image = {static_cast<float>(from.width), static_cast<float>(from.height), columns > 0.0F ? rows / columns : 1.0F, flips},
        .fade = {draw.opacity, draw.straighten ? 1.0F : 0.0F, 0.0F, 0.0F},
    };
    applyBlock(RendererState::kFragmentBlock, fragment);
    state.getGpu().draw(0, 4, 1);
    ++state.stats.drawCalls;
}

// The canvas stops its pass, copies the image it drew so far and goes on with a pass that keeps it, where the image of the group blends with the copy and replaces the pixels it covers. Only the scene of a canvas without lights holds such a group.
void FrameSubmitter::drawBackdrop(const Canvas& canvas, const Command& command, const Matrix& matrix, const math::Rect& passRect) {
    const LitTargets& targets = state.litTargets[canvas.litIndex];
    endPass();
    copy(targets.scene, targets.backdrop);
    beginOffscreenPass(targets.scene, std::nullopt);
    state.getGpu().applyViewport(passRect);
    state.getGpu().applyPipeline(state.getPipeline(Program::Backdrop, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen));
    applyClip(canvas, command.clip, passRect);

    const BlendDraw& draw = state.blends[command.first];
    const graphics::TextureResource& image = *draw.blend.from.getResource();
    const graphics::TextureResource& mask = *draw.blend.to.getResource();
    gpu::Device::Bindings bindings;
    bindings.vertexBuffers[0] = state.quad;
    bindings.textures[RendererState::kBackdropImage] = image.image;
    bindings.textures[RendererState::kBackdropMask] = mask.image;
    bindings.textures[RendererState::kBackdropCopy] = targets.backdrop.getTexture().getResource()->image;
    bindings.samplers[0] = image.sampler;
    state.getGpu().applyBindings(bindings);

    BackdropVertexParams vertex{.viewProjection = matrix};
    vertex.placement[0] = draw.origin.x;
    vertex.placement[1] = draw.origin.y;
    vertex.placement[2] = draw.rows.x;
    vertex.placement[3] = draw.rows.y;
    vertex.columns[0] = draw.columns.x;
    vertex.columns[1] = draw.columns.y;
    applyBlock(RendererState::kVertexBlock, vertex);
    const float mode = static_cast<float>(static_cast<int>(command.blend) - static_cast<int>(graphics::BlendMode::Type::Overlay));
    const BackdropFragmentParams fragment{
        .settings = {mode, draw.blend.progress, draw.blend.reversed ? 1.0F : 0.0F, draw.opacity},
        .flips = {image.flipped ? 1.0F : 0.0F, mask.flipped ? 1.0F : 0.0F, 0.0F, 0.0F},
    };
    applyBlock(RendererState::kFragmentBlock, fragment);
    state.getGpu().draw(0, 4, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::drawMetaball(const Command& command, const Matrix& matrix) {
    const MetaballDraw& draw = state.metaballs[command.first];
    const graphics::TextureResource& field = *state.fields[draw.field].getTexture().getResource();

    gpu::Device::Bindings bindings;
    bindings.vertexBuffers[0] = state.quad;
    bindings.textures[RendererState::kMetaballField] = field.image;
    bindings.samplers[0] = field.sampler;
    state.getGpu().applyBindings(bindings);

    MetaballVertexParams vertex{.viewProjection = matrix};
    vertex.area[0] = draw.area.x;
    vertex.area[1] = draw.area.y;
    vertex.area[2] = draw.area.width;
    vertex.area[3] = draw.area.height;
    vertex.field[0] = field.flipped ? 1.0F : 0.0F;
    vertex.field[1] = RendererState::expectsPremultiplied(command.blend) ? 1.0F : 0.0F;
    applyBlock(RendererState::kVertexBlock, vertex);

    const Renderer::MetaballStyle& style = draw.style;
    const MetaballFragmentParams fragment{
        .fill = {style.color.r, style.color.g, style.color.b, style.color.a},
        .outlineColor = {style.outlineColor.r, style.outlineColor.g, style.outlineColor.b, style.outlineColor.a},
        .levels = {style.threshold, style.outlineWidth, 0.0F, 0.0F},
    };
    applyBlock(RendererState::kFragmentBlock, fragment);
    state.getGpu().draw(0, 4, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::applyUniforms(const Matrix& matrix, graphics::BlendMode::Type blend) {
    const VertexParams params{.viewProjection = matrix, .premultiply = RendererState::expectsPremultiplied(blend) ? 1.0F : 0.0F};
    applyBlock(RendererState::kVertexBlock, params);
}

// Alpha and opaque draws cover the surface and the info by straight alpha and premultiplied draws by premultiplied alpha, while adding, multiplying and screen draws leave them, and multiplying draws also leave the emission.
void FrameSubmitter::applyShade(const Shade& shade, graphics::PassTarget pass, Program program, graphics::BlendMode::Type blend) {
    if (pass == graphics::PassTarget::LitScene) {
        using Mode = graphics::BlendMode::Type;
        const float covers = blend == Mode::Alpha || blend == Mode::Opaque ? 1.0F : (blend == Mode::Premultiplied ? 2.0F : 0.0F);
        const LitParams lit{
            .surface = {shade.specular, shade.emission, shade.unshaded ? 1.0F : 0.0F, covers},
            .info = {static_cast<float>(shade.lightMask) / 255.0F, static_cast<float>(shade.layer + 128) / 255.0F, shade.normalMap != nullptr ? shade.shininess / 255.0F : 0.0F, blend == Mode::Multiply ? 1.0F : 0.0F},
        };
        applyBlock(RendererState::kLitBlock, lit);
    }
    if (shade.material == nullptr) {
        return;
    }

    // Each uniform block of the material shader that the program declares takes its part of the copied values.
    const graphics::ShaderResource::Compiled& made = state.getMaterialProgram(*shade.material, program, pass);
    std::size_t offset = shade.uniformBegin;
    for (const graphics::Shader::Block& block : shade.material->shader.getBlocks()) {
        if (std::find(made.blocks.begin(), made.blocks.end(), block.slot) != made.blocks.end()) {
            state.getGpu().applyUniforms(block.slot, std::as_bytes(std::span(state.uniformBytes).subspan(offset, block.size)));
        }
        offset += block.size;
    }
}

void FrameSubmitter::bindBatch(gpu::Device::Bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass) {
    bindings.textures[RendererState::kSpriteTexture] = texture.image;
    for (std::size_t index = 0; index < shade.batchTextures.size(); ++index) {
        bindings.textures[RendererState::kBatchTextures + index] = shade.batchTextures[index]->image;
    }
    bindings.samplers[RendererState::kSpriteSampler] = texture.sampler;
    if (pass == graphics::PassTarget::LitScene) {
        bindings.textures[RendererState::kNormalTexture] = (shade.normalMap != nullptr ? shade.normalMap : state.white.getResource().get())->image;
    }
}

void FrameSubmitter::bindMaterial(gpu::Device::Bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass, Program program) {
    const graphics::ShaderResource::Compiled& made = state.getMaterialProgram(*shade.material, program, pass);
    const std::vector<graphics::Shader::TextureSlot>& textures = shade.material->shader.getTextures();
    // clang-format off
    const auto textureAt = [&](int slot) -> const graphics::TextureResource& {
        for (std::size_t index = 0; index < textures.size(); ++index) {
            if (textures[index].slot == slot) {
                return *state.shadeTextures[shade.textureBegin + index];
            }
        }
        if (program == Program::Batch && slot >= RendererState::kBatchTextures) {
            return *shade.batchTextures[static_cast<std::size_t>(slot - RendererState::kBatchTextures)];
        }
        return slot == 0 ? texture : *state.white.getResource();
    };
    // clang-format on

    for (const int slot : made.textures) {
        bindings.textures[static_cast<std::size_t>(slot)] = textureAt(slot).image;
    }
    for (const auto& [sampler, view] : made.samplers) {
        bindings.samplers[static_cast<std::size_t>(sampler)] = textureAt(view).sampler;
    }
}

} // namespace haylen::graphics2d
