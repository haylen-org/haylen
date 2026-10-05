#include "haylen/2d/graphics/Renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "2d/graphics/FrameSubmitter.hpp"
#include "2d/graphics/RendererState.hpp"
#include "2d/graphics/TextPainter.hpp"
#include "2d/lighting/ShadowMap.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/2d/lighting/Occluder.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/RichText.hpp"
#include "shaders/blend.glsl.h"
#include "shaders/blend_lit.glsl.h"
#include "shaders/composite.glsl.h"
#include "shaders/light.glsl.h"
#include "shaders/mesh.glsl.h"
#include "shaders/mesh_lit.glsl.h"
#include "shaders/metaball.glsl.h"
#include "shaders/metaball_lit.glsl.h"
#include "shaders/sprite.glsl.h"
#include "shaders/sprite_lit.glsl.h"
#include "shaders/text.glsl.h"
#include "shaders/text_lit.glsl.h"

namespace haylen::graphics2d {

const Renderer::CanvasOptions Renderer::kDefaultCanvas{};
const Renderer::MetaballStyle Renderer::kDefaultMetaballs{};

std::vector<std::uint8_t> Renderer::radialFalloff(int size) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size * size * 4), 255);
    const float half = static_cast<float>(size) * 0.5F;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float distanceToCenter = std::hypot(static_cast<float>(x) + 0.5F - half, static_cast<float>(y) + 0.5F - half) / half;
            pixels[static_cast<std::size_t>((y * size + x) * 4 + 3)] = static_cast<std::uint8_t>(lighting2d::Light::falloff(distanceToCenter) * 255.0F);
        }
    }
    return pixels;
}

// The kernel of a soft circle, `(1 - d^2)^2` over the distance `d` from its center as a fraction of its reach, which sums smoothly with its neighbors.
std::vector<std::uint8_t> Renderer::metaballKernel(int size) {
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size * size * 4), 255);
    const float half = static_cast<float>(size) * 0.5F;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float distanceToCenter = std::hypot(static_cast<float>(x) + 0.5F - half, static_cast<float>(y) + 0.5F - half) / half;
            const float inside = std::max(0.0F, 1.0F - distanceToCenter * distanceToCenter);
            pixels[static_cast<std::size_t>((y * size + x) * 4 + 3)] = static_cast<std::uint8_t>(std::lround(inside * inside * 255.0F));
        }
    }
    return pixels;
}

void Renderer::validatePostProcess(const CanvasOptions& options) {
    if (!options.postProcess) {
        return;
    }
    for (const Material& material : options.postProcess->materials) {
        if (!material.isValid()) {
            throw std::invalid_argument("A post-processing material needs a shader.");
        }
    }
}

float Renderer::lowestPoint(std::span<const math::Vec2> points) noexcept {
    float lowest = points.front().y;
    for (const math::Vec2 point : points) {
        lowest = std::max(lowest, point.y);
    }
    return lowest;
}

Renderer::Renderer(graphics::Device& device, core::JobSystem& jobs) : state(std::make_unique<RendererState>(device, jobs)) {
    constexpr std::array<float, 8> kCorners = {0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 1.0F, 1.0F, 1.0F};
    sg_buffer_desc quadDesc{};
    quadDesc.data = SG_RANGE(kCorners);
    quadDesc.label = "haylen-quad";
    state->quad = graphics::Gpu::makeBuffer(quadDesc);

    constexpr std::uint16_t kFull = 65535;
    const std::array<GpuInstance, 2> screen{
        GpuInstance{.position = {0.0F, 0.0F}, .size = {1.0F, 1.0F}, .uv = {0, 0, kFull, kFull}, .color = 0xFFFFFFFFU, .flash = 0, .rotation = 0.0F, .parameters = {0, 0, 0, 0}, .pivot = {0.0F, 0.0F}},
        GpuInstance{.position = {0.0F, 0.0F}, .size = {1.0F, 1.0F}, .uv = {0, kFull, kFull, 0}, .color = 0xFFFFFFFFU, .flash = 0, .rotation = 0.0F, .parameters = {0, 0, 0, 0}, .pivot = {0.0F, 0.0F}},
    };
    sg_buffer_desc screenDesc{};
    screenDesc.data = SG_RANGE(screen);
    screenDesc.label = "haylen-screen";
    state->screen = graphics::Gpu::makeBuffer(screenDesc);

    const auto make = [](const sg_shader_desc* (*description)(sg_backend)) { return graphics::Gpu::makeShader(*graphics::Gpu::selectShader(description)); };
    const auto at = [](Program program) { return static_cast<std::size_t>(program); };
    state->shaders[at(Program::Sprite)] = make(sprite_sprite_shader_desc);
    state->shaders[at(Program::Text)] = make(text_text_shader_desc);
    state->shaders[at(Program::Mesh)] = make(mesh_mesh_shader_desc);
    state->shaders[at(Program::ImageBlend)] = make(blend_blend_shader_desc);
    state->shaders[at(Program::Composite)] = make(composite_composite_shader_desc);
    state->shaders[at(Program::Light)] = make(light_light_shader_desc);
    state->shaders[at(Program::Metaball)] = make(metaball_metaball_shader_desc);
    state->litShaders[at(Program::Sprite)] = make(sprite_lit_sprite_shader_desc);
    state->litShaders[at(Program::Text)] = make(text_lit_text_shader_desc);
    state->litShaders[at(Program::Mesh)] = make(mesh_lit_mesh_shader_desc);
    state->litShaders[at(Program::ImageBlend)] = make(blend_lit_blend_shader_desc);
    state->litShaders[at(Program::Metaball)] = make(metaball_lit_metaball_shader_desc);

    // Light maps hold light in floating point where the backend renders and blends it, so lights can brighten the scene beyond its unlit colors.
    const sg_pixelformat_info hdr = sg_query_pixelformat(SG_PIXELFORMAT_RGBA16F);
    state->lightFormat = hdr.render && hdr.blend ? SG_PIXELFORMAT_RGBA16F : device.getState().offscreenFormat;

    state->white = device.getWhiteTexture();
    state->light = device.createTexture(graphics::Image(RendererState::kLightTextureSize, RendererState::kLightTextureSize, radialFalloff(RendererState::kLightTextureSize)), {.filter = graphics::Texture::Filter::Linear});
    state->metaball = device.createTexture(graphics::Image(RendererState::kMetaballTextureSize, RendererState::kMetaballTextureSize, metaballKernel(RendererState::kMetaballTextureSize)), {.filter = graphics::Texture::Filter::Linear});
}

Renderer::~Renderer() {
    for (const auto& [key, pipeline] : state->pipelines) {
        sg_destroy_pipeline(pipeline);
    }
    for (const sg_shader shader : state->shaders) {
        sg_destroy_shader(shader);
    }
    for (const sg_shader shader : state->litShaders) {
        sg_destroy_shader(shader);
    }
    if (state->shadowImage.id != SG_INVALID_ID) {
        sg_destroy_view(state->shadowView);
        sg_destroy_image(state->shadowImage);
    }

    for (sg_buffer buffer : {state->quad, state->screen, state->instanceBuffer, state->vertexBuffer, state->indexBuffer}) {
        if (buffer.id != SG_INVALID_ID) {
            sg_destroy_buffer(buffer);
        }
    }
}

void Renderer::beginFrame(const graphics::Viewport& viewport, math::Color clearColor) {
    state->pixelRect = viewport.getPixelRect();
    state->visibleRect = viewport.getVisibleRect();
    state->clearColor = clearColor;
    state->stats = {};
}

void Renderer::beginWorld(const Camera& camera, const CanvasOptions& options) {
    if (options.clear && !options.ambientLight && !options.postProcess) {
        throw std::invalid_argument("World canvases only support a clear color with lighting or post-processing.");
    }
    validatePostProcess(options);

    if (camera.viewport && !(camera.viewport->width > 0.0F && camera.viewport->height > 0.0F)) {
        throw std::invalid_argument("A camera viewport needs a positive width and height.");
    }

    const math::Rect area = camera.getViewRect(state->visibleRect);
    state->openCanvas({.kind = Canvas::Kind::World, .options = options, .viewSize = area.getSize(), .view = camera.viewTransform(area.getSize()), .frame = Canvas::frameOf(area, state->visibleRect)});
}

void Renderer::beginScreen(const CanvasOptions& options) {
    if (options.ambientLight || options.postProcess) {
        throw std::invalid_argument("Screen canvases do not support lighting or post-processing.");
    }
    if (options.clear) {
        throw std::invalid_argument("Screen canvases do not support a clear color.");
    }

    state->openCanvas({.kind = Canvas::Kind::Screen, .options = options, .viewSize = state->visibleRect.getSize(), .view = math::Transform2D::translation(-state->visibleRect.getMin())});
}

void Renderer::beginTarget(const graphics::RenderTarget& target, const Camera& camera, const CanvasOptions& options) {
    if (!target.isValid()) {
        throw std::invalid_argument("A render target canvas needs a valid render target.");
    }
    validatePostProcess(options);

    if (camera.viewport && !(camera.viewport->width > 0.0F && camera.viewport->height > 0.0F)) {
        throw std::invalid_argument("A camera viewport needs a positive width and height.");
    }

    const math::Rect whole{0.0F, 0.0F, target.getSize().x, target.getSize().y};
    const math::Rect area = camera.getViewRect(whole);
    state->openCanvas({.kind = Canvas::Kind::Target, .options = options, .target = target, .viewSize = area.getSize(), .view = camera.viewTransform(area.getSize()), .frame = Canvas::frameOf(area, whole)});
}

void Renderer::beginCapture(const graphics::RenderTarget& target, math::Color clear) {
    if (!target.isValid()) {
        throw std::invalid_argument("A capture needs a valid render target.");
    }

    state->closeCanvas();
    state->openCaptures.push_back(state->captures.size());
    state->captures.push_back({.target = target, .clear = clear});
}

void Renderer::endCapture() {
    if (state->openCaptures.empty()) {
        throw std::logic_error("The \"endCapture\" call has no matching \"beginCapture\".");
    }
    state->closeCapture();
}

void Renderer::draw(const Sprite& sprite) {
    if (!sprite.texture.isValid()) {
        throw std::invalid_argument("Cannot draw a sprite without a texture.");
    }
    if (!state->accepts(sprite.order)) {
        return;
    }

    const math::Rect source = sprite.source.isEmpty() ? math::Rect{0.0F, 0.0F, sprite.texture.getSize().x, sprite.texture.getSize().y} : sprite.source;
    const math::Vec2 size = (sprite.size.isZero() ? source.getSize() : sprite.size) * sprite.scale;
    const SpriteInstance instance{
        .position = sprite.position,
        .size = size,
        .source = source,
        .pivot = sprite.pivot,
        .rotation = sprite.rotation,
        .color = sprite.color,
        .flash = sprite.flash,
        .flip = sprite.flip,
    };
    const GpuInstance gpu = GpuInstance::make(*sprite.texture.getResource(), instance);
    state->addInstances(Program::Sprite, sprite.order, sprite.texture, std::span(&gpu, 1), sprite.position.y);
}

template <typename SpriteAt> void Renderer::addBatch(const graphics::Texture& texture, std::size_t count, const DrawOrder& order, const SpriteAt& spriteAt) {
    if (!texture.isValid()) {
        throw std::invalid_argument("Cannot draw a sprite batch without a texture.");
    }
    if (count == 0 || !state->accepts(order)) {
        return;
    }

    // A y-sorted canvas sorts every sprite of the batch on its own, so each one becomes an item.
    state->retain(texture);
    const auto first = static_cast<std::uint32_t>(state->instances.size());
    if (state->getCanvas().options.sort == SortMode::Y) {
        for (std::size_t index = 0; index < count; ++index) {
            DrawItem& item = state->addItem(Program::Sprite, order, texture.getResource().get(), spriteAt(index).position.y);
            item.first = first + static_cast<std::uint32_t>(index);
            item.count = 1;
        }
    } else {
        DrawItem& item = state->addItem(Program::Sprite, order, texture.getResource().get(), spriteAt(0).position.y);
        item.first = first;
        item.count = static_cast<std::uint32_t>(count);
    }
    state->instances.resize(state->instances.size() + count);

    // Large batches convert their instances on the worker pool while the frame thread takes its own share.
    GpuInstance* output = state->instances.data() + first;
    const graphics::TextureResource& resource = *texture.getResource();
    // clang-format off
    const auto convert = [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            output[index] = GpuInstance::make(resource, spriteAt(index));
        }
    };
    // clang-format on

    if (count >= RendererState::kParallelThreshold) {
        state->jobs.parallelFor(0, count, RendererState::kParallelGrain, convert);
    } else {
        convert(0, count);
    }
    state->stats.sprites += count;
}

void Renderer::drawBatch(const graphics::Texture& texture, std::span<const SpriteInstance> sprites, const DrawOrder& order) {
    addBatch(texture, sprites.size(), order, [sprites](std::size_t index) -> const SpriteInstance& { return sprites[index]; });
}

void Renderer::drawBatch(const graphics::Texture& texture, std::span<const float> values, const SpriteLayout& layout, const DrawOrder& order) {
    addBatch(texture, layout.getCount(values), order, [values, &layout](std::size_t index) { return layout.makeSprite(values, index); });
}

void Renderer::drawStatic(const StaticSpriteBatch& batch, const DrawOrder& order, math::Vec2 offset) {
    if (!batch.isValid() || batch.size() == 0 || !state->accepts(order)) {
        return;
    }

    state->retain(batch.getTexture());
    if (state->retainedSet.insert(batch.getResource().get()).second) {
        state->retainedBatches.push_back(batch.getResource());
    }

    DrawItem& item = state->addItem(Program::Sprite, order, batch.getTexture().getResource().get(), batch.getBounds().getBottom() + offset.y);
    item.batch = batch.getResource().get();
    item.offset = offset;
    item.count = static_cast<std::uint32_t>(batch.size());
    state->stats.sprites += batch.size();
}

void Renderer::drawNineSlice(const NineSlice& slice, const math::Rect& area, math::Color color, const DrawOrder& order, float borderScale) {
    if (!slice.isValid()) {
        throw std::invalid_argument("Cannot draw a nine-slice without a texture.");
    }
    if (!state->accepts(order)) {
        return;
    }

    std::vector<NineSlice::Patch>& patches = state->patches;
    patches.clear();
    slice.layout(area, borderScale, patches);
    if (patches.empty()) {
        return;
    }

    const graphics::TextureResource& resource = *slice.texture.getResource();
    std::vector<GpuInstance>& quads = state->scratchInstances;
    quads.clear();
    for (const NineSlice::Patch& patch : patches) {
        quads.push_back(GpuInstance::make(resource, {.position = patch.area.getMin(), .size = patch.area.getSize(), .source = patch.source, .pivot = {}, .color = color}));
    }
    state->addInstances(Program::Sprite, order, slice.texture, quads, area.getBottom());
}

void Renderer::drawText(text::Font& font, std::string_view content, math::Vec2 position, const text::Style& style, const DrawOrder& order) {
    if (state->accepts(order)) {
        drawTextLayout(*font.layout(content, style), position, style, order);
    }
}

void Renderer::drawText(text::FontFamily& family, std::string_view content, math::Vec2 position, const text::Style& style, const DrawOrder& order) {
    if (state->accepts(order)) {
        drawTextLayout(*family.layout(content, style), position, style, order);
    }
}

// The fonts of the layout upload the glyphs it added before its instances draw.
void Renderer::drawTextLayout(const text::Layout& layout, math::Vec2 position, const text::Style& style, const DrawOrder& order) {
    for (const text::Layout::Look& look : layout.looks) {
        look.font->sync();
    }
    TextPainter painter(state->white);
    painter.paintText(layout, position, style);
    for (const TextPainter::Batch& batch : painter.getBatches()) {
        state->addInstances(batch.program, order, batch.texture, batch.instances, position.y);
    }
}

// Rich text draws the frame of this moment, with its effects and reveal, and uploads the glyphs its fonts added.
void Renderer::drawRichText(text::RichText& richText, math::Vec2 position, const DrawOrder& order, math::Vec2 scale, math::Color tint) {
    if (!state->accepts(order)) {
        return;
    }

    const text::Layout& frame = richText.getFrame();
    for (const text::Layout::Look& look : frame.looks) {
        look.font->sync();
    }
    TextPainter painter(state->white);
    painter.paintRichText(frame, position, scale, tint);
    for (const TextPainter::Batch& batch : painter.getBatches()) {
        state->addInstances(batch.program, order, batch.texture, batch.instances, position.y + frame.size.y * scale.y);
    }
}

void Renderer::drawMesh(const graphics::Texture& texture, std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices, const DrawOrder& order) {
    if (!state->accepts(order)) {
        return;
    }

    // Render targets of backends that start at the bottom row are flipped, like sprites flip them, so meshes sample them upright.
    const graphics::Texture& used = texture.isValid() ? texture : state->white;
    const bool flipped = used.getResource()->flipped;
    std::vector<GpuVertex> packed;
    packed.reserve(vertices.size());
    for (const MeshVertex& vertex : vertices) {
        packed.push_back({{vertex.position.x, vertex.position.y}, {vertex.uv.x, flipped ? 1.0F - vertex.uv.y : vertex.uv.y}, vertex.color.toRgba8()});
    }
    for (const std::uint32_t index : indices) {
        if (index >= vertices.size()) {
            throw std::out_of_range("Mesh index refers to a vertex that does not exist.");
        }
    }
    state->addMesh(used, packed, indices, order);
}

void Renderer::drawImageBlend(const ImageBlend& blend, const DrawOrder& order) {
    if (!blend.from.isValid() || !blend.to.isValid()) {
        throw std::invalid_argument("An image blend needs a texture to blend from and one to blend to.");
    }
    if (!state->accepts(order)) {
        return;
    }

    state->requireReadable(blend.to.getResource().get());
    state->retain(blend.from);
    state->retain(blend.to);
    DrawItem& item = state->addItem(Program::ImageBlend, order, blend.from.getResource().get(), blend.area.getBottom());
    item.first = static_cast<std::uint32_t>(state->blends.size());
    item.count = 1;
    state->blends.push_back(blend);
}

void Renderer::drawLight(const lighting2d::Light& light) {
    if (!state->getCanvas().isLit()) {
        throw std::logic_error("Lights can only be drawn in a lit canvas, a world or render target canvas with ambient light.");
    }
    light.validate();
    if (!light.enabled) {
        return;
    }

    if (light.texture.isValid()) {
        state->retain(light.texture);
    }
    state->lights.push_back({.light = light});
    ++state->stats.lights;
}

void Renderer::drawOccluder(const lighting2d::Occluder& occluder) {
    if (!state->getCanvas().isLit()) {
        throw std::logic_error("Occluders can only be drawn in a lit canvas, a world or render target canvas with ambient light.");
    }
    occluder.validate();
    lighting2d::ShadowMap::appendSegments(occluder, state->segments);
    ++state->stats.occluders;
}

void Renderer::drawMetaballs(std::span<const math::Vec2> points, float radius, const MetaballStyle& style, const DrawOrder& order) {
    if (!(radius > 0.0F && std::isfinite(radius))) {
        throw std::invalid_argument("A metaball radius must be positive.");
    }
    if (!(style.threshold > 0.0F && style.threshold < 1.0F)) {
        throw std::invalid_argument("A metaball threshold must be between 0 and 1.");
    }
    if (!(style.outlineWidth >= 0.0F && style.threshold + style.outlineWidth < 1.0F)) {
        throw std::invalid_argument("A metaball outline width must be zero or more and keep the threshold plus the width below 1.");
    }
    if (points.empty() || !state->accepts(order)) {
        return;
    }

    // Every point splats one soft circle whose kernel reaches the threshold of 0.5 at the radius.
    const float reach = radius * kMetaballReach;
    math::Rect area = math::Rect::fromCenter(points.front(), math::Vec2{});
    for (const math::Vec2 point : points) {
        area = area.merged(math::Rect::fromCenter(point, {reach * 2.0F, reach * 2.0F}));
    }

    // The item goes first, so a draw it rejects leaves no field behind.
    DrawItem& item = state->addItem(Program::Metaball, order, state->metaball.getResource().get(), area.getBottom());
    item.first = static_cast<std::uint32_t>(state->metaballs.size());
    item.count = 1;
    state->retain(state->metaball);

    const graphics::TextureResource& kernel = *state->metaball.getResource();
    const auto first = static_cast<std::uint32_t>(state->splats.size());
    for (const math::Vec2 point : points) {
        state->splats.push_back(GpuInstance::make(kernel, {.position = point, .size = {reach * 2.0F, reach * 2.0F}}));
    }
    state->metaballs.push_back({.first = first, .count = static_cast<std::uint32_t>(points.size()), .area = area, .style = style});
}

void Renderer::drawRect(const math::Rect& rect, math::Color color, const DrawOrder& order) {
    if (!state->accepts(order)) {
        return;
    }
    const GpuInstance gpu = GpuInstance::make(*state->white.getResource(), {.position = rect.getMin(), .size = rect.getSize(), .pivot = {}, .color = color});
    state->addInstances(Program::Sprite, order, state->white, std::span(&gpu, 1), rect.getBottom());
}

// The four edges are one draw, so a y-sorted canvas keeps them together.
void Renderer::drawRectOutline(const math::Rect& rect, float thickness, math::Color color, const DrawOrder& order) {
    if (!state->accepts(order)) {
        return;
    }

    const graphics::TextureResource& white = *state->white.getResource();
    const std::array<math::Rect, 4> edges{math::Rect{rect.x, rect.y, rect.width, thickness}, math::Rect{rect.x, rect.getBottom() - thickness, rect.width, thickness}, math::Rect{rect.x, rect.y + thickness, thickness, rect.height - thickness * 2.0F}, math::Rect{rect.getRight() - thickness, rect.y + thickness, thickness, rect.height - thickness * 2.0F}};
    std::array<GpuInstance, 4> quads{};
    for (std::size_t index = 0; index < edges.size(); ++index) {
        quads[index] = GpuInstance::make(white, {.position = edges[index].getMin(), .size = edges[index].getSize(), .pivot = {}, .color = color});
    }
    state->addInstances(Program::Sprite, order, state->white, quads, rect.getBottom());
}

void Renderer::drawLine(math::Vec2 from, math::Vec2 to, float thickness, math::Color color, const DrawOrder& order) {
    const std::array<math::Vec2, 2> points{from, to};
    drawPolyline(points, thickness, color, false, order);
}

// Every segment is a rotated quad of the white texture, and the whole line is one draw.
void Renderer::drawPolyline(std::span<const math::Vec2> points, float thickness, math::Color color, bool closed, const DrawOrder& order) {
    if (points.size() < 2 || !state->accepts(order)) {
        return;
    }

    const graphics::TextureResource& white = *state->white.getResource();
    std::vector<GpuInstance> segments;
    segments.reserve(points.size());
    const std::size_t count = closed && points.size() > 2 ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        const math::Vec2 from = points[index];
        const math::Vec2 delta = points[(index + 1) % points.size()] - from;
        if (delta.isZero()) {
            continue;
        }
        segments.push_back(GpuInstance::make(white, {.position = from, .size = {delta.getLength(), thickness}, .pivot = {0.0F, 0.5F}, .rotation = delta.getAngle(), .color = color}));
    }
    if (!segments.empty()) {
        state->addInstances(Program::Sprite, order, state->white, segments, lowestPoint(points));
    }
}

void Renderer::drawCircle(math::Vec2 center, float radius, math::Color color, const DrawOrder& order, int segments) {
    if (!state->accepts(order)) {
        return;
    }

    const int count = segments > 0 ? segments : std::clamp(static_cast<int>(radius * 0.5F), 16, 96);
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(static_cast<std::size_t>(count + 1));
    indices.reserve(static_cast<std::size_t>(count * 3));

    const std::uint32_t packed = color.toRgba8();
    vertices.push_back({{center.x, center.y}, {0.5F, 0.5F}, packed});
    for (int index = 0; index < count; ++index) {
        const math::Vec2 point = center + math::Vec2::fromAngle(math::Math::kTau * static_cast<float>(index) / static_cast<float>(count), radius);
        vertices.push_back({{point.x, point.y}, {0.5F, 0.5F}, packed});
        indices.insert(indices.end(), {0U, static_cast<std::uint32_t>(index + 1), static_cast<std::uint32_t>((index + 1) % count + 1)});
    }
    state->addMesh(state->white, vertices, indices, order);
}

void Renderer::drawRing(math::Vec2 center, float radius, float thickness, math::Color color, const DrawOrder& order, int segments) {
    drawArc(center, radius, thickness, 0.0F, math::Math::kTau, color, order, segments);
}

void Renderer::drawArc(math::Vec2 center, float radius, float thickness, float startAngle, float endAngle, math::Color color, const DrawOrder& order, int segments) {
    if (!state->accepts(order)) {
        return;
    }

    const float sweep = endAngle - startAngle;
    const int count = segments > 0 ? segments : std::clamp(static_cast<int>(radius * std::fabs(sweep) / math::Math::kTau * 0.5F), 8, 96);
    const float inner = std::max(0.0F, radius - thickness * 0.5F);
    const float outer = radius + thickness * 0.5F;
    const std::uint32_t packed = color.toRgba8();

    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(static_cast<std::size_t>((count + 1) * 2));
    indices.reserve(static_cast<std::size_t>(count * 6));
    for (int index = 0; index <= count; ++index) {
        const float angle = startAngle + sweep * static_cast<float>(index) / static_cast<float>(count);
        const math::Vec2 innerPoint = center + math::Vec2::fromAngle(angle, inner);
        const math::Vec2 outerPoint = center + math::Vec2::fromAngle(angle, outer);
        vertices.push_back({{innerPoint.x, innerPoint.y}, {0.5F, 0.5F}, packed});
        vertices.push_back({{outerPoint.x, outerPoint.y}, {0.5F, 0.5F}, packed});
        if (index > 0) {
            const auto base = static_cast<std::uint32_t>((index - 1) * 2);
            indices.insert(indices.end(), {base, base + 1, base + 3, base, base + 3, base + 2});
        }
    }
    state->addMesh(state->white, vertices, indices, order);
}

void Renderer::drawPolygon(std::span<const math::Vec2> points, math::Color color, const DrawOrder& order) {
    if (!state->accepts(order)) {
        return;
    }

    const std::vector<std::uint32_t> indices = math::Geometry::triangulate(points);
    const std::uint32_t packed = color.toRgba8();
    std::vector<GpuVertex> vertices;
    vertices.reserve(points.size());
    for (const math::Vec2 point : points) {
        vertices.push_back({{point.x, point.y}, {0.5F, 0.5F}, packed});
    }
    state->addMesh(state->white, vertices, indices, order);
}

void Renderer::pushClip(const math::Rect& rect) {
    (void)state->getCanvas();
    math::Rect clip = rect;
    if (!state->clipStack.empty()) {
        clip = clip.intersection(state->clips[state->clipStack.back() - 1U]);
    }

    // Draws clipped to the same rectangle one after the other share its index, so they can share a draw call.
    if (state->clips.empty() || !(state->clips.back() == clip)) {
        state->clips.push_back(clip);
    }
    state->clipStack.push_back(static_cast<std::uint32_t>(state->clips.size()));
}

void Renderer::popClip() {
    if (state->clipStack.empty()) {
        throw std::logic_error("The \"popClip\" call has no matching \"pushClip\".");
    }
    state->clipStack.pop_back();
}

void Renderer::pushLayerOffset(int offset) {
    (void)state->getCanvas();
    state->layerOffsets.push_back(state->layerOffset);
    state->layerOffset += offset;
}

void Renderer::popLayerOffset() {
    if (state->layerOffsets.empty()) {
        throw std::logic_error("The \"popLayerOffset\" call has no matching \"pushLayerOffset\".");
    }
    state->layerOffset = state->layerOffsets.back();
    state->layerOffsets.pop_back();
}

StaticSpriteBatch Renderer::createStaticBatch(const graphics::Texture& texture, std::span<const SpriteInstance> sprites) {
    if (!texture.isValid() || sprites.empty()) {
        throw std::invalid_argument("A static batch needs a texture and at least one sprite.");
    }

    std::vector<GpuInstance> packed(sprites.size());
    math::Rect bounds = math::Rect::fromCenter(sprites.front().position, math::Vec2{});
    for (std::size_t index = 0; index < sprites.size(); ++index) {
        const SpriteInstance& sprite = sprites[index];
        packed[index] = GpuInstance::make(*texture.getResource(), sprite);
        const math::Vec2 topLeft = sprite.position - sprite.size * sprite.pivot;
        bounds = bounds.merged(math::Rect{topLeft.x, topLeft.y, sprite.size.x, sprite.size.y});
    }

    sg_buffer_desc desc{};
    desc.data = {.ptr = packed.data(), .size = packed.size() * sizeof(GpuInstance)};
    desc.label = "haylen-static-batch";

    auto resource = std::make_shared<StaticBatchResource>();
    resource->buffer = graphics::Gpu::makeBuffer(desc);
    resource->count = sprites.size();
    resource->texture = texture;
    resource->bounds = bounds;
    resource->graveyard = state->device.getState().graveyard;
    return StaticSpriteBatch(std::move(resource));
}

const graphics::Texture& Renderer::getLightTexture() const noexcept {
    return state->light;
}

bool Renderer::isHdrLighting() const noexcept {
    return state->lightFormat == SG_PIXELFORMAT_RGBA16F;
}

const Renderer::Stats& Renderer::getStats() const noexcept {
    return state->stats;
}

math::Rect Renderer::getCanvasBounds() const noexcept {
    return state->canvasOpen ? state->canvases.back().getWorldBounds() : math::Rect{};
}

float Renderer::getCanvasUnitSize() const {
    const Canvas& canvas = state->getCanvas();
    return 1.0F / std::sqrt(std::fabs(canvas.view.getDeterminant()));
}

bool Renderer::isCapturing() const noexcept {
    return !state->openCaptures.empty();
}

void Renderer::endFrame(const graphics::FrameTarget& target) {
    state->closeCanvas();
    while (!state->openCaptures.empty()) {
        state->closeCapture();
    }
    try {
        FrameSubmitter(*state, target).submit();
    } catch (...) {
        state->resetFrame();
        throw;
    }
    state->resetFrame();
}

} // namespace haylen::graphics2d
