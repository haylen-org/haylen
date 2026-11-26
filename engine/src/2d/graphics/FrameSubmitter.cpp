#include "2d/graphics/FrameSubmitter.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <numeric>
#include <optional>

#include "2d/graphics/MaterialResource.hpp"
#include "2d/graphics/RendererState.hpp"
#include "2d/graphics/StaticBatchResource.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/RenderTargetResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/gpu/SurfaceTurn.hpp"
#include "haylen/graphics/Device.hpp"

namespace haylen::graphics2d {

const PostProcess FrameSubmitter::kNoPostProcess{};

template <typename Block> void FrameSubmitter::applyBlock(int slot, const Block& block) {
    static_assert(sizeof(VertexParams) == RendererState::kVertexBlockSize && sizeof(DrawParams) == RendererState::kDrawBlockSize && sizeof(LitParams) == RendererState::kLitBlockSize && sizeof(LightParams) == 64 && sizeof(CompositeParams) == 176 && sizeof(FilterParams) == 32 && sizeof(TurnParams) == 16);
    static_assert(sizeof(BlendVertexParams) == 112 && sizeof(BlendFragmentParams) == 80 && sizeof(BackdropVertexParams) == 96 && sizeof(BackdropFragmentParams) == 32 && sizeof(MetaballVertexParams) == 96 && sizeof(MetaballFragmentParams) == 48 && sizeof(GlassParams) == 48);
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
    return (item.program == Program::Sprite || item.program == Program::Text || item.program == Program::PlainText || item.program == Program::Recolor || item.program == Program::Effect || item.program == Program::Shape || item.program == Program::Batch || item.program == Program::GradientText) && item.batch == nullptr;
}

void FrameSubmitter::submit() {
    prepareTargets();
    castShadows();
    buildCommands();
    upload();

    // A draw can still fail inside a pass, such as a material whose program the GPU device rejects, so the pass and the frame end before the error goes on and the next frame starts clean.
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
    std::size_t glassCount = 0;
    bool screenGlass = false;
    for (Canvas& canvas : state.canvases) {
        const math::Vec2 size = state.getPassRect(canvas).getSize();
        if (canvas.isComposited()) {
            canvas.litIndex = litCount++;
            canvas.uniformLighting = canvas.isLit() && takesLightsAlike(canvas);
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

        if (canvas.glass) {
            screenGlass = prepareGlass(canvas, glassCount) || screenGlass;
        }
    }

    // Targets past what this frame uses go back to the device, so a peak such as a transition between two lit scenes does not keep its images.
    state.litTargets.resize(litCount);
    state.fields.resize(fieldCount);
    state.glassTargets.resize(glassCount);

    // The screen draws into an image of its own only while its canvases hold glass.
    const int screenWidth = std::max(1, static_cast<int>(std::lround(state.framebufferSize.x)));
    const int screenHeight = std::max(1, static_cast<int>(std::lround(state.framebufferSize.y)));
    if (!screenGlass) {
        state.screenImage = {};
    } else if (!state.screenImage.isValid() || state.screenImage.getWidth() != screenWidth || state.screenImage.getHeight() != screenHeight) {
        state.screenImage = state.device.createRenderTarget(screenWidth, screenHeight, {.filter = graphics::Texture::Filter::Nearest});
    }
}

// The draws of one shade take the lights alike, and distortion draws never reach the scene. A light reaches a draw when its item mask shares a bit with the light mask of the draw and the layer of the draw lies in its range.
bool FrameSubmitter::takesLightsAlike(const Canvas& canvas) const {
    std::optional<std::uint32_t> checked;
    for (std::size_t index = canvas.itemBegin; index < canvas.itemEnd; ++index) {
        const DrawItem& item = state.items[index];
        if (item.distortion || checked == item.shade) {
            continue;
        }

        checked = item.shade;
        const Shade& shade = state.shades[item.shade];
        if (shade.material != nullptr || shade.normalMap != nullptr || shade.emission > 0.0F || shade.unshaded) {
            return false;
        }

        for (std::size_t light = canvas.lightBegin; light < canvas.lightEnd; ++light) {
            const lighting2d::Light& drawn = state.lights[light].light;
            if (drawn.affects(shade.lightMask, shade.layer) != drawn.affects(kBackgroundLightMask, kBackgroundLayer)) {
                return false;
            }
        }
    }

    return true;
}

// Destinations of one size share their images of glass, which the frames that draw over the same sizes keep, in the order the sizes first appear.
bool FrameSubmitter::prepareGlass(const Canvas& canvas, std::size_t& glassTargetsUsed) {
    const math::Vec2 size = getDestinationSize(canvas);
    const int width = std::max(1, static_cast<int>(std::lround(size.x)));
    const int height = std::max(1, static_cast<int>(std::lround(size.y)));
    std::size_t index = 0;
    while (index < glassTargetsUsed && (state.glassTargets[index].image.getWidth() != width || state.glassTargets[index].image.getHeight() != height)) {
        ++index;
    }

    if (index == glassTargetsUsed) {
        ++glassTargetsUsed;
        if (state.glassTargets.size() < glassTargetsUsed) {
            state.glassTargets.resize(glassTargetsUsed);
        }

        GlassTargets& targets = state.glassTargets[index];
        if (!targets.image.isValid() || targets.image.getWidth() != width || targets.image.getHeight() != height) {
            targets = {};
            targets.image = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
        }
    }

    for (std::size_t item = canvas.itemBegin; item < canvas.itemEnd; ++item) {
        if (state.items[item].program == Program::Glass) {
            state.glasses[state.items[item].first].targets = index;
        }
    }

    return !canvas.isComposited() && canvas.kind != Canvas::Kind::Target && canvas.capture == 0;
}

// A composited canvas draws into its scene, which has the size of its pass, and other canvases draw into their render target, their capture or the screen.
math::Vec2 FrameSubmitter::getDestinationSize(const Canvas& canvas) const {
    if (canvas.isComposited()) {
        return state.getPassRect(canvas).getSize();
    }

    if (canvas.kind == Canvas::Kind::Target) {
        return canvas.target.getSize();
    }

    if (canvas.capture != 0) {
        return state.captures[canvas.capture - 1].target.getSize();
    }

    return state.framebufferSize;
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
        targets.light = state.device.getState().createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear}, {.colors = {state.lightFormat}});
    }

    if (canvas.isLit() && !canvas.uniformLighting && !targets.info.isValid()) {
        targets.emission = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.surface = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.info = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
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

    // The blur of the distortion map, bloom and blur start at half the size, which their filters read with linear filtering, and their chains grow the smaller sizes they need when they blur.
    for (auto [used, chain] : {std::pair{canvas.distorted, &targets.distortionBlur}, std::pair{canvas.hasBloom(), &targets.bloom}, std::pair{canvas.hasBlur(), &targets.blur}}) {
        if (used && chain->empty()) {
            growChain(*chain, 1, width, height);
        }
    }

    return targets;
}

// Every size of a chain is half the one before it, starting at half the size of the canvas.
void FrameSubmitter::growChain(std::vector<std::array<graphics::RenderTarget, 2>>& chain, std::size_t levels, int width, int height) {
    while (chain.size() < levels) {
        const int shift = static_cast<int>(chain.size()) + 1;
        const int levelWidth = std::max(1, width >> shift);
        const int levelHeight = std::max(1, height >> shift);
        std::array<graphics::RenderTarget, 2>& pair = chain.emplace_back();
        for (graphics::RenderTarget& image : pair) {
            image = state.device.createRenderTarget(levelWidth, levelHeight, {.filter = graphics::Texture::Filter::Linear});
        }
    }
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

        state.shadowImage = state.getGpu().makeImage({.width = lighting2d::ShadowMap::kRowTexels, .height = state.shadowRows, .format = gpu::Device::PixelFormat::R32Float, .kind = gpu::Device::ImageKind::Dynamic, .label = "haylen-shadow-atlas"});
    }

    state.shadowTexels.assign(rows * lighting2d::ShadowMap::kRowTexels, lighting2d::ShadowMap::kClear);
    // clang-format off
    state.jobs.parallelFor(0, casts.size(), 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            const auto& [draw, canvas] = casts[index];
            const std::span<float> row(state.shadowTexels.data() + index * lighting2d::ShadowMap::kRowTexels, lighting2d::ShadowMap::kRowTexels);
            const std::span<const lighting2d::ShadowMap::Segment> segments(state.segments.data() + canvas->segmentBegin, canvas->segmentEnd - canvas->segmentBegin);
            draw->axis = lighting2d::ShadowMap::cast(draw->light, segments, canvas->getWorldBounds(), row);
        }
    });
    // clang-format on

    // The rows of the lights of the frame go to the GPU with the dynamic textures of the frame, and the rows past them, which no light of the frame samples, keep what they held.
    const gpu::Device::Region region{0, 0, lighting2d::ShadowMap::kRowTexels, static_cast<int>(rows)};
    shadowWrite = gpu::Device::ImageWrite{.image = state.shadowImage, .region = region, .pixels = std::as_bytes(std::span(state.shadowTexels))};
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
    Command command{.program = item.program, .blend = item.blend, .clip = item.clip, .fade = item.fade, .shade = item.shade, .texture = item.texture, .batch = item.batch, .offset = item.offset, .count = item.count};

    if (item.program == Program::Mesh) {
        command.first = static_cast<std::uint32_t>(state.uploadIndices.size());
        const auto first = state.indices.begin() + static_cast<std::ptrdiff_t>(item.first);
        state.uploadIndices.insert(state.uploadIndices.end(), first, first + static_cast<std::ptrdiff_t>(item.count));
    } else if (item.program == Program::ImageBlend || item.program == Program::Backdrop || item.program == Program::Metaball || item.program == Program::Glass) {
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
        const bool sameState = previous.program == command.program && previous.blend == command.blend && previous.clip == command.clip && previous.fade == command.fade && sameShade && previous.texture == command.texture;
        const bool single = command.program == Program::ImageBlend || command.program == Program::Backdrop || command.program == Program::Metaball || command.program == Program::Glass;
        const bool mergeable = !single && previous.batch == nullptr && command.batch == nullptr && previous.first + previous.count == command.first;
        if (sameState && mergeable) {
            previous.count += command.count;
            return;
        }
    }

    state.commands.push_back(command);
}

// The splats of metaball fields and the shapes of glass follow the instances of the draws in the same buffer.
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

    uploaded = *source;
    splatBase = source->size();
    glassBase = splatBase + state.splats.size();
    const std::size_t total = glassBase + state.glassInstances.size();
    if (total > 0) {
        state.ensureBuffer(state.instanceBuffer, state.instanceCapacity, total, sizeof(GpuInstance), false);
    }

    writeInstances(*source, 0);
    writeInstances(state.splats, splatBase);
    writeInstances(state.glassInstances, glassBase);
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

void FrameSubmitter::writeInstances(std::span<const GpuInstance> data, std::size_t offset) {
    if (data.empty()) {
        return;
    }

    state.getGpu().writeBuffer(state.instanceBuffer, offset * sizeof(GpuInstance), std::as_bytes(data));
    state.stats.uploadedBytes += data.size_bytes();
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
        state.getGpu().applyPipeline(state.getPipeline(Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Additive), graphics::PassTarget::Offscreen, passLayout));
        applyUniforms(matrix, graphics::BlendMode::Type::Additive, state.fields[draw.field].getSize());
        applyDraw(0, {}, getTextureForm(kernel));

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
            drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, rect, &canvas.target);
            endPass();
        }

        return;
    }

    LitTargets& targets = state.litTargets[canvas.litIndex];
    const math::Rect full{0.0F, 0.0F, static_cast<float>(targets.scene.getWidth()), static_cast<float>(targets.scene.getHeight())};
    const math::Color clear = canvas.options.clear.value_or(canvas.kind == Canvas::Kind::Target ? math::Color::transparent() : state.clearColor).getPremultiplied();

    // The distortion map blurs by a deviation that grows with the bend distance, so the slope the composite bends the image along stays smooth and never folds the image into copies of itself, whatever the sharp edges of the textures of the distortion draws.
    if (canvas.distortEnd > canvas.distortBegin) {
        const PostProcess& post = canvas.options.postProcess ? *canvas.options.postProcess : kNoPostProcess;
        beginOffscreenPass(targets.distortion, math::Color::transparent());
        drawCommands(canvas, canvas.distortBegin, canvas.distortEnd, graphics::PassTarget::Offscreen, full, &targets.distortion);
        endPass();
        blurInto(targets.distortion, targets.distortionBlur, -1.0F, post.distortion * kBendDeviation / kBlurDeviation * full.width / std::max(canvas.viewSize.x, 1.0F));
    }

    // The draws of a canvas whose draws take the lights alike color its scene as they would without light, which is what the color image of a lit pass holds.
    if (canvas.isLit() && !canvas.uniformLighting) {
        beginLitPass(targets, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::LitScene, full, &targets.scene);
        endPass();
    } else {
        beginOffscreenPass(targets.scene, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, full, &targets.scene);
        endPass();
    }

    if (canvas.isLit()) {
        renderLights(canvas, targets);
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

// Neighbouring lights that share a blend mode, a shape and whether they cast shadows draw as the instances of one call, in the order the app drew them, where only the lights with shadows take the program that reads the shadow map. A canvas whose draws take the lights alike skips the lights that miss its background and lights every pixel without reading what covers it.
void FrameSubmitter::renderLights(const Canvas& canvas, const LitTargets& targets) {
    beginOffscreenPass(targets.light, *canvas.options.ambientLight);
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    const LightParams vertex{.viewProjection = matrix};
    const gpu::Device::Sampler nearest = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});

    const bool uniform = canvas.uniformLighting;
    std::size_t first = canvas.lightBegin;
    while (first < canvas.lightEnd) {
        if (uniform && !state.lights[first].light.affects(kBackgroundLightMask, kBackgroundLayer)) {
            ++first;
            continue;
        }

        const lighting2d::Light& light = state.lights[first].light;
        const graphics::TextureResource& shape = getLightShape(light);
        const bool shadowed = state.lights[first].shadowRow >= 0;
        std::size_t end = first + 1;
        while (end < canvas.lightEnd && state.lights[end].light.blend == light.blend && &getLightShape(state.lights[end].light) == &shape && (state.lights[end].shadowRow >= 0) == shadowed && (!uniform || state.lights[end].light.affects(kBackgroundLightMask, kBackgroundLayer))) {
            ++end;
        }

        const Program shaded = shadowed ? Program::ShadowedLight : Program::Light;
        const Program program = uniform ? (shadowed ? Program::ShadowedUniformLight : Program::UniformLight) : shaded;
        state.getGpu().applyPipeline(state.getPipeline(program, static_cast<std::uint8_t>(light.blend), graphics::PassTarget::LightMap, passLayout));
        gpu::Device::Bindings bindings;
        bindings.vertexBuffers[0] = state.quad;
        bindings.vertexBuffers[1] = state.lightBuffer;
        bindings.vertexOffsets[1] = static_cast<int>(first * sizeof(LightInstance));
        bindings.textures[RendererState::kLightShape] = shape.image;
        bindings.textures[RendererState::kLightShadow] = state.shadowImage;
        bindings.samplers[RendererState::kLightShapeSampler] = shape.sampler;
        bindings.samplers[RendererState::kLightShadowSampler] = nearest;
        if (!uniform) {
            bindings.textures[RendererState::kLightSurface] = targets.surface.getTexture().getResource()->image;
            bindings.textures[RendererState::kLightInfo] = targets.info.getTexture().getResource()->image;
            bindings.samplers[RendererState::kLightSurfaceSampler] = nearest;
        }

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
void FrameSubmitter::renderPostChain(Canvas& canvas, LitTargets& targets) {
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

        const float pixels = targets.post[0].getSize().x / std::max(canvas.viewSize.x, 1.0F);
        if (canvas.hasBloom()) {
            blurInto(targets.post[0], targets.bloom, post.bloomThreshold, post.bloomRadius * pixels);
        }

        if (canvas.hasBlur()) {
            blurInto(targets.post[0], targets.blur, -1.0F, post.blur * pixels);
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
    drawCanvases(index + 1, graphics::PassTarget::Offscreen, &capture.target);
    endPass();
}

// The frame of an opaque window clears its alpha to 1 and never writes it, whatever the clear color and the blends of the app, and a transparent window holds premultiplied colors. A screen whose canvases hold glass draws them into its image first, which the glass reads, and the swapchain pass draws that image.
void FrameSubmitter::renderSwapchain() {
    const math::Color clear = transparent ? state.clearColor.getPremultiplied() : state.clearColor.withAlpha(1.0F);
    const graphics::PassTarget pass = transparent ? graphics::PassTarget::TransparentSwapchain : graphics::PassTarget::Swapchain;
    const bool imaged = state.screenImage.isValid();
    if (imaged) {
        beginOffscreenPass(state.screenImage, clear);
        drawCanvases(0, graphics::PassTarget::Offscreen, &state.screenImage);
        endPass();
    }

    openPass({.load = gpu::Device::Load::Clear, .clear = clear, .label = "haylen-swapchain-pass"}, {});

    if (imaged) {
        drawScreenImage(pass);
    } else {
        drawCanvases(0, pass, nullptr);
    }

    endPass();
    renderPresentations();
    state.getGpu().commit();
}

// Each presented image covers the largest area of its aspect that fits in the middle of its surface, sampled from its pixels as they are when it fits them one to one.
void FrameSubmitter::renderPresentations() {
    for (const Presentation& presentation : state.presentations) {
        openPass({.load = gpu::Device::Load::Clear, .clear = presentation.clear.withAlpha(1.0F), .label = "haylen-presentation-pass", .surface = presentation.surface}, {});

        const math::Vec2 image = presentation.target.getSize();
        const float fit = std::min(presentation.size.x / image.x, presentation.size.y / image.y);
        const math::Vec2 shown{std::round(image.x * fit), std::round(image.y * fit)};
        state.getGpu().applyViewport({std::round((presentation.size.x - shown.x) * 0.5F), std::round((presentation.size.y - shown.y) * 0.5F), shown.x, shown.y});
        state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Swapchain, passLayout));
        gpu::Device::Bindings bindings;
        bindings.textures[RendererState::kFilterSource] = presentation.target.getTexture().getResource()->image;
        bindings.samplers[0] = state.device.getState().getSampler({.filter = shown == image ? graphics::Texture::Filter::Nearest : graphics::Texture::Filter::Linear});
        state.getGpu().applyBindings(bindings);
        const FilterParams params{.settings = {2.0F, 0.0F, state.getGpu().getCapabilities().originTopLeft ? 0.0F : 1.0F, 0.0F}, .offset = {}};
        applyTurn();
        applyBlock(0, params);
        state.getGpu().draw(0, 3, 1);
        ++state.stats.drawCalls;
        endPass();
    }
}

void FrameSubmitter::drawScreenImage(graphics::PassTarget pass) {
    state.getGpu().applyViewport({0.0F, 0.0F, state.screenImage.getSize().x, state.screenImage.getSize().y});
    state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), pass, passLayout));
    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kFilterSource] = state.screenImage.getTexture().getResource()->image;
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});
    state.getGpu().applyBindings(bindings);

    const FilterParams params{.settings = {2.0F, 0.0F, state.getGpu().getCapabilities().originTopLeft ? 0.0F : 1.0F, 0.0F}, .offset = {}};
    applyTurn();
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
}

// Draws the world and screen canvases of a destination, the screen or a capture, by their order and then in the order they began, which is their place in the list of canvases. A stable sort would take a buffer of its own in some standard libraries.
void FrameSubmitter::drawCanvases(std::size_t capture, graphics::PassTarget pass, const graphics::RenderTarget* image) {
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

        drawCommands(*canvas, canvas->sceneBegin, canvas->sceneEnd, pass, rect, image);
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

// A pass without a clear color keeps what the target holds, such as a canvas that goes on after a group read what lies below it. A pass draws into the first color of a render target, into its samples when it has several, which resolve into its texture as the pass ends and stay for a later pass unless the target is memoryless, and leaves its other colors and its depth as they are.
void FrameSubmitter::beginOffscreenPass(const graphics::RenderTarget& renderTarget, std::optional<math::Color> clear) {
    const graphics::RenderTargetResource& target = *renderTarget.getResource();
    gpu::Device::ColorAttachment attachment{.image = target.getColorAttachment(0), .load = clear ? gpu::Device::Load::Clear : gpu::Device::Load::Keep, .clear = clear.value_or(math::Color::transparent())};
    if (!target.samples.empty()) {
        attachment.store = target.options.memoryless ? gpu::Device::Store::Discard : gpu::Device::Store::Store;
        attachment.resolve = target.textures.front().getResource()->image;
    }

    openPass({.colors = std::span(&attachment, 1), .label = "haylen-offscreen-pass"}, {.color = target.textures.front().getResource()->format, .samples = target.options.samples});
}

void FrameSubmitter::openPass(const gpu::Device::PassDesc& desc, const graphics::PassLayout& layout) {
    state.getGpu().beginPass(desc);
    passOpen = true;
    passLayout = layout;
    ++state.stats.passes;
}

// A tile GPU loads none of what a pass that discards its target held, and the pixels around the region of such a pass hold whatever the GPU leaves there.
void FrameSubmitter::beginFilterPass(const graphics::RenderTarget& destination, const std::optional<math::Rect>& region, bool keeps) {
    if (!region) {
        beginOffscreenPass(destination, math::Color::transparent());
        return;
    }

    if (keeps) {
        beginOffscreenPass(destination, std::nullopt);
        return;
    }

    const gpu::Device::ColorAttachment attachment{.image = destination.getTexture().getResource()->image, .load = gpu::Device::Load::DontCare};
    openPass({.colors = std::span(&attachment, 1), .label = "haylen-region-pass"}, {});
}

// The info image starts with the light mask 1 and the layer 0 of draws that set neither, so lights reach the background like such a draw.
void FrameSubmitter::beginLitPass(const LitTargets& targets, math::Color clear) {
    const std::array<gpu::Device::ColorAttachment, 4> attachments{{
        {.image = targets.scene.getTexture().getResource()->image, .clear = clear},
        {.image = targets.emission.getTexture().getResource()->image, .clear = math::Color(0.0F, 0.0F, 0.0F, 0.0F)},
        {.image = targets.surface.getTexture().getResource()->image, .clear = math::Color(0.5F, 0.5F, 0.0F, 0.0F)},
        {.image = targets.info.getTexture().getResource()->image, .clear = math::Color(static_cast<float>(kBackgroundLightMask) / 255.0F, static_cast<float>(kBackgroundLayer + 128) / 255.0F, 0.0F, 0.0F)},
    }};
    openPass({.colors = attachments, .label = "haylen-lit-scene-pass"}, {});
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
    const graphics::RenderTarget& image = stage != Stage::Finish ? targets.scene : (canvas.hasBlur() ? targets.blur.front()[0] : targets.post[0]);
    const auto viewOf = [](const graphics::RenderTarget& used) { return used.getTexture().getResource()->image; };

    // A canvas without distortion, chromatic aberration, bloom, blur or a lookup texture takes the program without those steps, and one without lighting also the program without the light map.
    const bool effects = distorts || blooms || grades || stage != Stage::Whole || post.chromaticAberration > 0.0F;
    const Program program = effects ? Program::Composite : (lit ? Program::LitComposite : Program::PlainComposite);
    state.getGpu().applyPipeline(state.getPipeline(program, static_cast<std::uint8_t>(graphics::BlendMode::Type::Premultiplied), pass, passLayout));
    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kCompositeScene] = viewOf(image);
    bindings.textures[RendererState::kCompositeLight] = viewOf(lit ? targets.light : image);
    bindings.textures[RendererState::kCompositeEmission] = lit && canvas.uniformLighting ? state.transparent.getResource()->image : viewOf(lit ? targets.emission : image);
    bindings.textures[RendererState::kCompositeDistortion] = viewOf(distorts ? targets.distortionBlur.front()[0] : image);
    bindings.textures[RendererState::kCompositeBloom] = viewOf(blooms ? targets.bloom.front()[0] : image);
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
        .flags = {lit ? 1.0F : 0.0F, state.getGpu().getCapabilities().originTopLeft ? 0.0F : 1.0F, stageFlag, distorts ? 1.0F : 0.0F},
        .view = {canvas.viewSize.x, canvas.viewSize.y, post.distortion, post.chromaticAberration},
        .shape = {post.pixelate, grades ? post.colorLut.getSize().y : 0.0F, distorts ? post.distortion * kBendDeviation : 0.0F, 0.0F},
        .visionRed = {vision[0][0], vision[0][1], vision[0][2], 0.0F},
        .visionGreen = {vision[1][0], vision[1][1], vision[1][2], 0.0F},
        .visionBlue = {vision[2][0], vision[2][1], vision[2][2], 0.0F},
        .grain = {post.grain, post.dither ? 1.0F : 0.0F, static_cast<float>(state.epoch % kGrainFrames), 0.0F},
    };
    applyTurn();
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::filter(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, float threshold, math::Vec2 step, const std::optional<math::Rect>& region, bool keeps) {
    const math::Vec2 size = source.getSize();
    beginFilterPass(destination, region, keeps);
    state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen, passLayout));
    if (region) {
        state.getGpu().applyScissor(*region);
    }

    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kFilterSource] = source.getTexture().getResource()->image;
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Linear});
    state.getGpu().applyBindings(bindings);

    const bool shrinks = step.isZero();
    const FilterParams params{
        .settings = {shrinks ? 0.0F : 1.0F, threshold, state.getGpu().getCapabilities().originTopLeft ? 0.0F : 1.0F, 0.0F},
        .offset = {step.x / size.x, step.y / size.y, 0.5F / size.x, 0.5F / size.y},
    };
    applyTurn();
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
    endPass();
}

void FrameSubmitter::copy(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, const std::optional<math::Rect>& region, bool keeps) {
    beginFilterPass(destination, region, keeps);
    state.getGpu().applyPipeline(state.getPipeline(Program::Filter, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen, passLayout));
    if (region) {
        state.getGpu().applyScissor(*region);
    }

    gpu::Device::Bindings bindings;
    bindings.textures[RendererState::kFilterSource] = source.getTexture().getResource()->image;
    bindings.samplers[0] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});
    state.getGpu().applyBindings(bindings);
    const FilterParams params{.settings = {2.0F, 0.0F, state.getGpu().getCapabilities().originTopLeft ? 0.0F : 1.0F, 0.0F}, .offset = {}};
    applyTurn();
    applyBlock(0, params);
    state.getGpu().draw(0, 3, 1);
    ++state.stats.drawCalls;
    endPass();
}

// Nine taps of the blur span four steps on each side, so the steps of a radius are a quarter of it in texels. The image shrinks into smaller sizes until the radius spans at most four texels, where the taps lie a texel or less apart and leave no gap for rings, blurs there and grows back size by size, each step averaging four taps around every texel. A blur of a region writes at each size the region moved out by a margin of texels of the smallest size, which holds every texel the taps reach from the region, so it costs what the region covers whatever the radius.
void FrameSubmitter::blurInto(const graphics::RenderTarget& image, std::vector<std::array<graphics::RenderTarget, 2>>& chain, float threshold, float radius, const std::optional<math::Rect>& region) {
    const std::size_t level = getBlurLevel(image.getWidth(), image.getHeight(), radius);
    growChain(chain, level + 1, image.getWidth(), image.getHeight());
    const float reach = std::ldexp(radius * chain.front()[0].getSize().x / image.getSize().x, -static_cast<int>(level));

    const std::optional<math::Rect> around = region ? std::optional<math::Rect>(region->expanded(kBlurMargin * std::ldexp(1.0F, static_cast<int>(level) + 1))) : std::nullopt;
    filter(image, chain[0][0], threshold, {}, getChainRegion(around, chain, 0));

    for (std::size_t index = 1; index <= level; ++index) {
        filter(chain[index - 1][0], chain[index][0], -1.0F, {}, getChainRegion(around, chain, index));
    }

    const float spacing = std::max(reach * 0.25F, 0.5F);
    filter(chain[level][0], chain[level][1], -1.0F, {spacing, 0.0F}, getChainRegion(around, chain, level));
    filter(chain[level][1], chain[level][0], -1.0F, {0.0F, spacing}, getChainRegion(around, chain, level));

    for (std::size_t index = level; index > 0; --index) {
        filter(chain[index][0], chain[index - 1][0], -1.0F, {}, getChainRegion(around, chain, index - 1));
    }
}

std::size_t FrameSubmitter::getBlurLevel(int width, int height, float radius) noexcept {
    const int firstWidth = std::max(1, width >> 1);
    const int firstHeight = std::max(1, height >> 1);
    float reach = radius * static_cast<float>(firstWidth) / static_cast<float>(width);
    std::size_t level = 0;
    while (reach > kBlurReach && level + 1 < kBlurLevels && std::min(firstWidth, firstHeight) >> (level + 1) >= kBlurSmallest) {
        reach *= 0.5F;
        ++level;
    }

    return level;
}

std::optional<math::Rect> FrameSubmitter::getChainRegion(const std::optional<math::Rect>& region, const std::vector<std::array<graphics::RenderTarget, 2>>& chain, std::size_t index) noexcept {
    if (!region) {
        return std::nullopt;
    }

    const float scale = std::ldexp(1.0F, -static_cast<int>(index) - 1);
    const math::Vec2 low = math::Vec2::floor(region->getMin() * scale);
    const math::Vec2 high{std::ceil(region->getRight() * scale), std::ceil(region->getBottom() * scale)};
    const math::Rect bounds{0.0F, 0.0F, chain[index][0].getSize().x, chain[index][0].getSize().y};
    return math::Rect::fromMinMax(low, high).intersection(bounds);
}

// A post-processing material draws the image as one sprite over the unit square, which covers the viewport of the pass.
void FrameSubmitter::drawPostMaterial(std::size_t shadeIndex, const graphics::Texture& image, graphics::PassTarget pass) {
    const Shade& shade = state.shades[shadeIndex];
    const graphics::TextureResource& resource = *image.getResource();
    state.getGpu().applyPipeline(state.getMaterialPipeline(*shade.material, Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Premultiplied), pass, passLayout));
    applyUniforms(gpu::SurfaceTurn::turnClip(projection(math::Transform2D::identity(), {1.0F, 1.0F}), state.getGpu().getPassTurns()), graphics::BlendMode::Type::Premultiplied, image.getSize());
    applyDraw(0, {}, getTextureForm(resource));
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
    const std::array<math::Vec2, 4> corners{clip.getMin(), math::Vec2{clip.getRight(), clip.getTop()}, clip.getMax(), math::Vec2{clip.getLeft(), clip.getBottom()}};
    const math::Rect scissor = getPixels(canvas, corners, passRect, false).intersection(passRect);
    return {scissor.x, scissor.y, std::max(0.0F, scissor.width), std::max(0.0F, scissor.height)};
}

math::Rect FrameSubmitter::getPixels(const Canvas& canvas, const std::array<math::Vec2, 4>& corners, const math::Rect& passRect, bool touched) noexcept {
    const math::Vec2 scale = passRect.getSize() / canvas.viewSize;
    std::array<math::Vec2, 4> placed = corners;
    for (math::Vec2& corner : placed) {
        corner = canvas.view.apply(corner);
    }

    const math::Rect units = math::Geometry2D::bounds(placed);
    const math::Vec2 low = passRect.getMin() + units.getMin() * scale;
    const math::Vec2 high = passRect.getMin() + units.getMax() * scale;
    if (touched) {
        return math::Rect::fromMinMax(math::Vec2::floor(low), {std::ceil(high.x), std::ceil(high.y)});
    }

    return math::Rect::fromMinMax(math::Vec2::round(low), math::Vec2::round(high));
}

// The clip 0 covers the whole pass.
math::Rect FrameSubmitter::getClipScissor(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) const noexcept {
    return clip == 0 ? passRect : getScissor(canvas, state.clips[clip - 1U], passRect);
}

bool FrameSubmitter::showsPixels(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) const noexcept {
    const math::Rect scissor = getClipScissor(canvas, clip, passRect);
    return scissor.width > 0.0F && scissor.height > 0.0F;
}

void FrameSubmitter::applyClip(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) {
    state.getGpu().applyScissor(getClipScissor(canvas, clip, passRect));
}

void FrameSubmitter::drawCommands(const Canvas& canvas, std::size_t begin, std::size_t end, graphics::PassTarget pass, const math::Rect& passRect, const graphics::RenderTarget* image) {
    const Matrix matrix = gpu::SurfaceTurn::turnClip(projection(canvas.view, canvas.viewSize), state.getGpu().getPassTurns());
    gpu::Device::Pipeline currentPipeline;
    graphics::TextureResource* currentTexture = nullptr;
    std::optional<std::uint32_t> currentClip;
    std::uint32_t currentShade = 0;
    std::uint32_t currentFade = 0;
    math::Vec2 currentOffset{};
    TextureForm currentForm = TextureForm::Levels;
    std::optional<std::uint32_t> checkedClip;
    bool clipShows = true;

    for (std::size_t index = begin; index < end; ++index) {
        const Command& command = state.commands[index];

        // A clip that covers no pixel of the pass hides its commands whole, so they draw nothing and change no state, and no device sees a scissor without pixels.
        if (checkedClip != command.clip) {
            checkedClip = command.clip;
            clipShows = showsPixels(canvas, command.clip, passRect);
        }

        if (!clipShows) {
            continue;
        }

        const Shade& shade = state.shades[command.shade];

        if (command.program == Program::Backdrop) {
            drawBackdrop(canvas, command, matrix, passRect);
            currentPipeline = {};
            currentClip.reset();
            continue;
        }

        if (command.program == Program::Glass) {
            if (image == nullptr) {
                throw std::logic_error("A glass draws only into a pass that draws into an image.");
            }

            if (!state.glasses[command.first].ready) {
                prepareGlasses(canvas, index, end, passRect, *image);
            }

            drawGlass(canvas, command, matrix, passRect);
            currentPipeline = {};
            currentClip.reset();
            continue;
        }

        if (command.program == Program::ImageBlend || command.program == Program::Metaball) {
            // These apply their own pipeline, uniforms and clip, so the next command applies them again.
            state.getGpu().applyPipeline(state.getPipeline(command.program, static_cast<std::uint8_t>(command.blend), pass, passLayout));
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
        const gpu::Device::Pipeline pipeline = shade.material != nullptr ? state.getMaterialPipeline(*shade.material, command.program, blend, pass, passLayout) : state.getPipeline(command.program, blend, pass, passLayout);
        const bool fresh = pipeline.id != currentPipeline.id;
        if (fresh) {
            state.getGpu().applyPipeline(pipeline);
            currentPipeline = pipeline;
            currentClip.reset();
        }

        const TextureForm form = command.program == Program::Batch ? TextureForm::Levels : getTextureForm(*command.texture);
        if (fresh || command.offset != currentOffset || command.fade != currentFade || form != currentForm) {
            applyUniforms(translated(matrix, command.offset), command.blend, passRect.getSize());
            applyDraw(command.fade, command.offset, form);
            currentOffset = command.offset;
            currentFade = command.fade;
            currentForm = form;
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

        const gpu::Device::Bindings bindings = makeBindings(command, shade, pass);
        state.getGpu().applyBindings(bindings);

        if (command.program == Program::Mesh) {
            state.getGpu().draw(static_cast<int>(command.first), static_cast<int>(command.count), 1);
        } else {
            state.getGpu().draw(0, 4, static_cast<int>(isPaired(command.program) ? command.count / 2 : command.count));
        }

        ++state.stats.drawCalls;
    }
}

bool FrameSubmitter::isPaired(Program program) noexcept {
    return program == Program::Recolor || program == Program::Effect || program == Program::Batch || program == Program::GradientText;
}

// A paired program reads the record after each instance through a second buffer of the same instances.
gpu::Device::Bindings FrameSubmitter::makeBindings(const Command& command, const Shade& shade, graphics::PassTarget pass) {
    gpu::Device::Bindings bindings;
    if (command.program == Program::Mesh) {
        bindings.vertexBuffers[0] = state.vertexBuffer;
        bindings.indexBuffer = state.indexBuffer;
    } else {
        bindings.vertexBuffers[0] = state.quad;
        bindings.vertexBuffers[1] = command.batch != nullptr ? command.batch->buffer : state.instanceBuffer;
        bindings.vertexOffsets[1] = static_cast<int>(command.first * sizeof(GpuInstance));
    }

    if (isPaired(command.program)) {
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

    return bindings;
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
    applyDraw(command.fade, {}, getTextureForm(from));

    const float flips = (from.flipped ? 1.0F : 0.0F) + (to.flipped ? 2.0F : 0.0F);
    const float rows = draw.rows.getLength();
    const float columns = draw.columns.getLength();
    const BlendFragmentParams fragment{
        .settings = {static_cast<float>(blend.pattern), blend.progress, blend.reversed ? 1.0F : 0.0F, blend.angle},
        .shape = {blend.center.x, blend.center.y, blend.cellSize, blend.blockSize},
        .color = {blend.color.r, blend.color.g, blend.color.b, blend.color.a},
        .image = {static_cast<float>(from.width), static_cast<float>(from.height), columns > 0.0F ? rows / columns : 1.0F, flips},
        .fade = {draw.opacity, draw.straighten ? 1.0F : 0.0F, draw.dither, 0.0F},
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
    state.getGpu().applyPipeline(state.getPipeline(Program::Backdrop, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), graphics::PassTarget::Offscreen, passLayout));
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
    applyDraw(command.fade, {}, getTextureForm(image));
    const float mode = static_cast<float>(static_cast<int>(command.blend) - static_cast<int>(graphics::BlendMode::Type::Overlay));
    const BackdropFragmentParams fragment{
        .settings = {mode, draw.blend.progress, draw.blend.reversed ? 1.0F : 0.0F, draw.opacity},
        .flips = {image.flipped ? 1.0F : 0.0F, mask.flipped ? 1.0F : 0.0F, 0.0F, 0.0F},
    };
    applyBlock(RendererState::kFragmentBlock, fragment);
    state.getGpu().draw(0, 4, 1);
    ++state.stats.drawCalls;
}

// A glass whose image behind it reads no pixel draws nothing.
void FrameSubmitter::prepareGlasses(const Canvas& canvas, std::size_t first, std::size_t end, const math::Rect& passRect, const graphics::RenderTarget& image) {
    const math::Rect bounds{0.0F, 0.0F, image.getSize().x, image.getSize().y};
    std::optional<math::Rect> drawn;
    std::size_t last = first;
    for (std::size_t index = first; index < end; ++index) {
        const Command& command = state.commands[index];
        if (!showsPixels(canvas, command.clip, passRect)) {
            continue;
        }

        if (command.program == Program::Glass) {
            GlassDraw& draw = state.glasses[command.first];
            const math::Rect clip = getClipScissor(canvas, command.clip, passRect);
            const math::Rect read = getPixels(canvas, draw.corners, passRect, true).intersection(clip.expanded(std::ceil(draw.refractionPixels))).intersection(bounds);
            const bool reads = read.width > 0.0F && read.height > 0.0F;
            const bool untouched = !drawn || !drawn->intersects(getGlassSource(read, draw.blurPixels, image));
            bool apart = true;
            for (std::size_t other = first; other < index && apart; ++other) {
                const Command& earlier = state.commands[other];
                apart = earlier.program != Program::Glass || !state.glasses[earlier.first].read.intersects(read);
            }

            // A glass that cannot join ends the group, so no glass of the group draws after another end of the pass rewrote the image behind it.
            if (index != first && reads && !(untouched && apart)) {
                break;
            }

            draw.read = reads ? read : math::Rect{};
            draw.ready = true;
            last = index;
        }

        const std::optional<math::Rect> pixels = getCommandPixels(canvas, command, passRect);
        if (!pixels) {
            break;
        }

        if (!pixels->isEmpty()) {
            drawn = drawn ? drawn->merged(*pixels) : *pixels;
        }
    }

    bool ended = false;
    for (std::size_t index = first; index <= last; ++index) {
        const Command& command = state.commands[index];
        if (command.program != Program::Glass || !state.glasses[command.first].ready || state.glasses[command.first].read.isEmpty()) {
            continue;
        }

        if (!ended) {
            endPass();
        }

        renderBehindGlass(state.glasses[command.first], image, ended);
        ended = true;
    }

    if (ended) {
        beginOffscreenPass(image, std::nullopt);
        state.getGpu().applyViewport(passRect);
    }
}

void FrameSubmitter::renderBehindGlass(const GlassDraw& draw, const graphics::RenderTarget& image, bool keeps) {
    GlassTargets& targets = state.glassTargets[draw.targets];
    if (draw.blurPixels > 0.0F) {
        blurInto(image, targets.chain, -1.0F, draw.blurPixels, draw.read);
        filter(targets.chain.front()[0], targets.image, -1.0F, {}, draw.read, keeps);
    } else {
        copy(image, targets.image, draw.read, keeps);
    }
}

math::Rect FrameSubmitter::getGlassSource(const math::Rect& read, float blurPixels, const graphics::RenderTarget& image) noexcept {
    if (blurPixels <= 0.0F) {
        return read;
    }

    const std::size_t level = getBlurLevel(image.getWidth(), image.getHeight(), blurPixels);
    return read.expanded(kBlurMargin * std::ldexp(1.0F, static_cast<int>(level) + 1));
}

std::optional<math::Rect> FrameSubmitter::getCommandPixels(const Canvas& canvas, const Command& command, const math::Rect& passRect) const {
    const std::optional<math::Rect> units = getCommandBounds(command);
    if (!units) {
        return std::nullopt;
    }

    const math::Rect moved = units->translated(command.offset);
    const std::array<math::Vec2, 4> corners{moved.getMin(), math::Vec2{moved.getRight(), moved.y}, moved.getMax(), math::Vec2{moved.x, moved.getBottom()}};
    return getPixels(canvas, corners, passRect, true).intersection(getClipScissor(canvas, command.clip, passRect));
}

// The quads of sprites, glyphs and shapes, the vertices of meshes and the bounds of baked batches hold what their draws change, while the quads of effects reach past their sprites by an outline and a glow, and image blends, groups and metaballs cover areas of their own.
std::optional<math::Rect> FrameSubmitter::getCommandBounds(const Command& command) const {
    if (command.program == Program::Glass) {
        return math::Geometry2D::bounds(state.glasses[command.first].corners);
    }

    if (command.batch != nullptr) {
        return command.batch->bounds;
    }

    std::optional<math::Rect> bounds;
    const auto merge = [&bounds](const math::Rect& next) { bounds = bounds ? bounds->merged(next) : next; };
    switch (command.program) {
    case Program::Mesh:
        for (std::size_t index = command.first; index < command.first + command.count; ++index) {
            const GpuVertex& vertex = state.vertices[state.uploadIndices[index]];
            merge(math::Rect{vertex.position[0], vertex.position[1], 0.0F, 0.0F});
        }

        return bounds;
    case Program::Shape:
        for (std::size_t index = command.first; index < command.first + command.count; ++index) {
            merge(uploaded[index].getShapeBounds());
        }

        return bounds;
    case Program::Sprite:
    case Program::Text:
    case Program::PlainText:
    case Program::Recolor:
    case Program::Batch:
    case Program::GradientText: {
        const std::size_t stride = isPaired(command.program) ? 2 : 1;
        for (std::size_t index = command.first; index < command.first + command.count; index += stride) {
            merge(uploaded[index].getBounds());
        }

        return bounds;
    }
    case Program::ImageBlend:
    case Program::Composite:
    case Program::Light:
    case Program::Metaball:
    case Program::Filter:
    case Program::Effect:
    case Program::Backdrop:
    case Program::Glass:
    case Program::LitComposite:
    case Program::PlainComposite:
    case Program::ShadowedLight:
    case Program::UniformLight:
    case Program::ShadowedUniformLight:
        break;
    }

    return std::nullopt;
}

// The shape draws over the image behind it, read at its own pixels, with the program of the glass or the shape program of its material.
void FrameSubmitter::drawGlass(const Canvas& canvas, const Command& command, const Matrix& matrix, const math::Rect& passRect) {
    const GlassDraw& draw = state.glasses[command.first];
    if (draw.read.isEmpty()) {
        return;
    }

    const GlassTargets& targets = state.glassTargets[draw.targets];
    const Shade& shade = state.shades[command.shade];
    const auto blend = static_cast<std::uint8_t>(command.blend);
    const graphics::PassTarget pass = graphics::PassTarget::Offscreen;
    state.getGpu().applyPipeline(shade.material != nullptr ? state.getMaterialPipeline(*shade.material, Program::Glass, blend, pass, passLayout) : state.getPipeline(Program::Glass, blend, pass, passLayout));
    applyClip(canvas, command.clip, passRect);
    const graphics::TextureResource& behind = *targets.image.getTexture().getResource();
    applyUniforms(matrix, command.blend, passRect.getSize());
    applyDraw(command.fade, {}, getTextureForm(behind));
    applyShade(shade, pass, Program::Glass, command.blend);

    gpu::Device::Bindings bindings;
    bindings.vertexBuffers[0] = state.quad;
    bindings.vertexBuffers[1] = state.instanceBuffer;
    bindings.vertexOffsets[1] = static_cast<int>((glassBase + draw.instance) * sizeof(GpuInstance));
    if (shade.material != nullptr) {
        bindMaterial(bindings, shade, behind, pass, Program::Glass);
    } else {
        bindings.textures[RendererState::kSpriteTexture] = behind.image;
        bindings.samplers[RendererState::kSpriteSampler] = behind.sampler;
        const Glass& glass = draw.glass;
        const GlassParams params{
            .look = {glass.saturation, glass.brightness, glass.refraction, glass.rim},
            .tint = {glass.tint.r, glass.tint.g, glass.tint.b, glass.tint.a},
            .rim = {glass.rimWidth, 0.0F, 0.0F, 0.0F},
        };
        applyBlock(RendererState::kFragmentBlock, params);
    }

    state.getGpu().applyBindings(bindings);
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
    applyDraw(command.fade, {}, getTextureForm(field));

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

// The triangle of the composite and filter programs covers the viewport, and turns with the images of a swapchain pass.
void FrameSubmitter::applyTurn() {
    const Matrix turned = gpu::SurfaceTurn::turnClip(kIdentity, state.getGpu().getPassTurns());
    const TurnParams params{.turn = {turned[0], turned[1], turned[4], turned[5]}};
    applyBlock(RendererState::kTurnBlock, params);
}

// The viewport of a pass whose images are turned a quarter takes the width of the window as its height.
void FrameSubmitter::applyUniforms(const Matrix& matrix, graphics::BlendMode::Type blend, math::Vec2 viewport) {
    const bool across = (state.getGpu().getPassTurns() & 1) != 0;
    const std::array<float, 2> pixels = across ? std::array{viewport.y, viewport.x} : std::array{viewport.x, viewport.y};
    const VertexParams params{.viewProjection = matrix, .premultiply = RendererState::expectsPremultiplied(blend) ? 1.0F : 0.0F, .viewport = pixels};
    applyBlock(RendererState::kVertexBlock, params);
}

void FrameSubmitter::applyDraw(std::uint32_t fade, math::Vec2 offset, TextureForm form) {
    DrawParams params;
    if (fade != 0) {
        const auto [begin, count] = state.fades[fade - 1U];
        for (std::uint32_t index = 0; index < count; ++index) {
            const FadeEdge& edge = state.fadeEdges[begin + index];
            const std::size_t lane = index % 4U;
            Vector& x = index < 4U ? params.x : params.moreX;
            Vector& y = index < 4U ? params.y : params.moreY;
            Vector& shift = index < 4U ? params.offset : params.moreOffset;
            x[lane] = edge.slope.x;
            y[lane] = edge.slope.y;
            shift[lane] = edge.getCoverage(offset);
        }

        params.fadeCount = static_cast<float>(count);
    }

    params.textureForm = static_cast<float>(form);
    applyBlock(RendererState::kDrawBlock, params);
}

// A texture without mipmaps, such as the atlas of text, a render target or a dynamic texture, has a single level.
FrameSubmitter::TextureForm FrameSubmitter::getTextureForm(const graphics::TextureResource& texture) const noexcept {
    if (&texture == state.white.getResource().get()) {
        return TextureForm::White;
    }

    return texture.options.mipmaps.value_or(false) ? TextureForm::Levels : TextureForm::SingleLevel;
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
    for (const int slot : made.textures) {
        bindings.textures[static_cast<std::size_t>(slot)] = getMaterialTexture(shade, texture, program, slot).image;
    }

    for (const auto& [sampler, view] : made.samplers) {
        bindings.samplers[static_cast<std::size_t>(sampler)] = getMaterialTexture(shade, texture, program, view).sampler;
    }
}

const graphics::TextureResource& FrameSubmitter::getMaterialTexture(const Shade& shade, const graphics::TextureResource& texture, Program program, int slot) const {
    const std::vector<graphics::Shader::TextureSlot>& textures = shade.material->shader.getTextures();
    for (std::size_t index = 0; index < textures.size(); ++index) {
        if (textures[index].slot == slot) {
            return *state.shadeTextures[shade.textureBegin + index];
        }
    }

    if (program == Program::Batch && slot >= RendererState::kBatchTextures) {
        return *shade.batchTextures[static_cast<std::size_t>(slot - RendererState::kBatchTextures)];
    }

    return slot == 0 ? texture : *state.white.getResource();
}

} // namespace haylen::graphics2d
