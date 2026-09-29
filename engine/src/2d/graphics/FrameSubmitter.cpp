#include "2d/graphics/FrameSubmitter.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <numeric>

#include "2d/graphics/MaterialResource.hpp"
#include "2d/graphics/RendererState.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/FrameTarget.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/RenderTargetResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "shaders/blend.glsl.h"
#include "shaders/composite.glsl.h"
#include "shaders/light.glsl.h"
#include "shaders/mesh.glsl.h"
#include "shaders/metaball.glsl.h"
#include "shaders/sprite.glsl.h"
#include "shaders/sprite_lit.glsl.h"

namespace haylen::graphics2d {

// Every program reads its view projection from slot 0 and the lighting of lit passes from slot 7, which the shader library reserves.
static_assert(UB_sprite_haylen_vs_params == 0 && UB_sprite_lit_haylen_lit_params == 7);

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
    return (item.program == Program::Sprite || item.program == Program::Text) && item.batch == nullptr;
}

math::Rect FrameSubmitter::getPassRect(const Canvas& canvas) const {
    math::Rect destination = state.pixelRect;
    if (canvas.kind == Canvas::Kind::Target) {
        destination = {0.0F, 0.0F, canvas.target.getSize().x, canvas.target.getSize().y};
    } else if (canvas.capture != 0) {
        const math::Vec2 size = state.captures[canvas.capture - 1].target.getSize();
        destination = {0.0F, 0.0F, size.x, size.y};
    }
    const math::Rect& frame = canvas.frame;
    return {destination.x + frame.x * destination.width, destination.y + frame.y * destination.height, frame.width * destination.width, frame.height * destination.height};
}

void FrameSubmitter::submit() {
    prepareTargets();
    castShadows();
    buildCommands();
    upload();
    renderOffscreen();
    renderSwapchain();
}

void FrameSubmitter::prepareTargets() {
    std::size_t litCount = 0;
    std::size_t fieldCount = 0;
    for (Canvas& canvas : state.canvases) {
        const math::Vec2 size = getPassRect(canvas).getSize();
        if (canvas.isComposited()) {
            canvas.litIndex = litCount++;
            getLitTargets(canvas.litIndex, size, canvas.isLit(), canvas.hasPostMaterials());
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
}

LitTargets& FrameSubmitter::getLitTargets(std::size_t index, math::Vec2 size, bool lit, bool post) {
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
    if (lit && !targets.light.isValid()) {
        targets.emission = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.surface = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.info = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Nearest});
        targets.light = state.device.getState().createRenderTarget(width, height, state.lightFormat, {.filter = graphics::Texture::Filter::Linear});
    }
    if (post && !targets.post[0].isValid()) {
        for (graphics::RenderTarget& image : targets.post) {
            image = state.device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
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

    struct Job {
        LightDraw* draw = nullptr;
        const Canvas* canvas = nullptr;
    };
    std::vector<Job> jobs;
    for (const Canvas& canvas : state.canvases) {
        for (std::size_t index = canvas.lightBegin; index < canvas.lightEnd; ++index) {
            LightDraw& draw = state.lights[index];
            if (draw.light.shadows) {
                draw.shadowRow = static_cast<int>(jobs.size());
                jobs.push_back({.draw = &draw, .canvas = &canvas});
            }
        }
    }

    const std::size_t rows = std::max<std::size_t>(jobs.size(), 1);
    state.shadowTexels.assign(rows * lighting2d::ShadowMap::kResolution, lighting2d::ShadowMap::kClear);
    // clang-format off
    state.jobs.parallelFor(0, jobs.size(), 1, [&](std::size_t begin, std::size_t end) {
        for (std::size_t index = begin; index < end; ++index) {
            const Job& job = jobs[index];
            const std::span<float> row(state.shadowTexels.data() + index * lighting2d::ShadowMap::kResolution, lighting2d::ShadowMap::kResolution);
            const std::span<const lighting2d::ShadowMap::Segment> segments(state.segments.data() + job.canvas->segmentBegin, job.canvas->segmentEnd - job.canvas->segmentBegin);
            job.draw->axis = lighting2d::ShadowMap::cast(job.draw->light, segments, job.canvas->getWorldBounds(), row);
        }
    });
    // clang-format on

    writeShadowAtlas(static_cast<int>(rows));
    state.stats.shadows = jobs.size();
}

// The atlas grows to the next power of two of rows, and the light pass samples it with texel fetches.
void FrameSubmitter::writeShadowAtlas(int rows) {
    if (state.shadowRows < rows) {
        if (state.shadowImage.id != SG_INVALID_ID) {
            state.device.getState().graveyard->bury(state.shadowImage, state.shadowView);
        }
        state.shadowRows = static_cast<int>(std::bit_ceil(static_cast<unsigned>(std::max(rows, 8))));

        sg_image_desc desc{};
        desc.usage.write_transient = true;
        desc.usage.immutable = false;
        desc.width = lighting2d::ShadowMap::kResolution;
        desc.height = state.shadowRows;
        desc.pixel_format = SG_PIXELFORMAT_R32F;
        desc.label = "haylen-shadow-atlas";
        state.shadowImage = graphics::Gpu::makeImage(desc);

        sg_view_desc viewDesc{};
        viewDesc.texture.image = state.shadowImage;
        state.shadowView = sg_make_view(&viewDesc);
    }

    sg_write_image_desc write{};
    write.src.data = {.ptr = state.shadowTexels.data(), .size = state.shadowTexels.size() * sizeof(float)};
    write.src.bytes_per_row = static_cast<int>(lighting2d::ShadowMap::kResolution * sizeof(float));
    write.src.bytes_per_slice = write.src.bytes_per_row * rows;
    write.dst.image = state.shadowImage;
    write.size = {.width = lighting2d::ShadowMap::kResolution, .height = rows, .num_slices = 1};
    sg_write_image_transient(&write);
    state.stats.uploadedBytes += write.src.data.size;
}

// Orders every canvas, groups compatible neighbours into commands and lays instance data out in draw order.
void FrameSubmitter::buildCommands() {
    std::vector<std::uint32_t> order;
    std::uint32_t expected = 0;
    bool inOrder = true;

    for (Canvas& canvas : state.canvases) {
        order.resize(canvas.itemEnd - canvas.itemBegin);
        std::iota(order.begin(), order.end(), static_cast<std::uint32_t>(canvas.itemBegin));
        // clang-format off
        std::sort(order.begin(), order.end(), [&](std::uint32_t lhs, std::uint32_t rhs) {
            const DrawItem& a = state.items[lhs];
            const DrawItem& b = state.items[rhs];
            return a.key != b.key ? a.key < b.key : a.sequence < b.sequence;
        });
        // clang-format on

        canvas.sceneBegin = state.commands.size();
        commandFloor = canvas.sceneBegin;
        for (const std::uint32_t index : order) {
            const DrawItem& item = state.items[index];
            if (isInstanced(item)) {
                inOrder = inOrder && item.first == expected;
                expected = item.first + item.count;
            }
            append(item);
        }
        canvas.sceneEnd = state.commands.size();
    }

    identity = inOrder && expected == state.instances.size();
}

void FrameSubmitter::append(const DrawItem& item) {
    Command command{.program = item.program, .blend = item.blend, .clip = item.clip, .shade = item.shade, .texture = item.texture, .batch = item.batch, .offset = item.offset, .count = item.count};

    if (item.program == Program::Mesh) {
        command.first = static_cast<std::uint32_t>(state.uploadIndices.size());
        const auto first = state.indices.begin() + static_cast<std::ptrdiff_t>(item.first);
        state.uploadIndices.insert(state.uploadIndices.end(), first, first + static_cast<std::ptrdiff_t>(item.count));
    } else if (item.program == Program::ImageBlend || item.program == Program::Metaball) {
        command.first = item.first;
    } else if (item.batch == nullptr) {
        command.first = static_cast<std::uint32_t>(instanceCursor);
        placements.emplace_back(item.first, item.count);
        instanceCursor += item.count;
    }

    // Neighbours that share state and continue each other's data become one draw call.
    if (state.commands.size() > commandFloor) {
        Command& previous = state.commands.back();
        const bool sameState = previous.program == command.program && previous.blend == command.blend && previous.clip == command.clip && previous.shade == command.shade && previous.texture == command.texture;
        const bool single = command.program == Program::ImageBlend || command.program == Program::Metaball;
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
    const std::vector<GpuInstance>* source = &state.instances;
    if (!identity) {
        state.upload.resize(instanceCursor);
        std::size_t cursor = 0;
        for (const auto& [first, count] : placements) {
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
    const auto write = [&](const GpuInstance* data, std::size_t count, std::size_t offset) {
        if (count == 0) {
            return;
        }
        sg_write_buffer_desc desc{};
        desc.src.data = {.ptr = data, .size = count * sizeof(GpuInstance)};
        desc.dst = {.buffer = state.instanceBuffer, .offset = offset * sizeof(GpuInstance)};
        sg_write_buffer_transient(&desc);
        state.stats.uploadedBytes += count * sizeof(GpuInstance);
    };
    // clang-format on
    write(source->data(), source->size(), 0);
    write(state.splats.data(), state.splats.size(), splatBase);
    writeBuffer(state.vertexBuffer, state.vertexCapacity, state.vertices.data(), state.vertices.size(), sizeof(GpuVertex), false);
    writeBuffer(state.indexBuffer, state.indexCapacity, state.uploadIndices.data(), state.uploadIndices.size(), sizeof(std::uint32_t), true);

    state.stats.instances = source->size();
    state.stats.vertices = state.vertices.size();
    state.stats.indices = state.uploadIndices.size();
}

void FrameSubmitter::writeBuffer(sg_buffer& buffer, std::size_t& capacity, const void* data, std::size_t count, std::size_t elementSize, bool indexBuffer) {
    if (count == 0) {
        return;
    }
    state.ensureBuffer(buffer, capacity, count, elementSize, indexBuffer);

    sg_write_buffer_desc desc{};
    desc.src.data = {.ptr = data, .size = count * elementSize};
    desc.dst.buffer = buffer;
    sg_write_buffer_transient(&desc);
    state.stats.uploadedBytes += count * elementSize;
}

// Passes run in the order the frame recorded them, so a capture renders once the canvases it holds have their offscreen passes, and before any later canvas that draws its texture.
void FrameSubmitter::renderOffscreen() {
    std::size_t nextCapture = 0;
    for (std::size_t index = 0; index < state.canvases.size(); ++index) {
        while (nextCapture < state.captures.size() && state.captures[nextCapture].canvasEnd <= index) {
            renderCapture(nextCapture++);
        }
        renderFields(state.canvases[index]);
        renderCanvasOffscreen(state.canvases[index]);
    }
    while (nextCapture < state.captures.size()) {
        renderCapture(nextCapture++);
    }
}

// Every point of a metaball draw adds its soft circle to the field, which covers the view of the canvas.
void FrameSubmitter::renderFields(const Canvas& canvas) {
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    const graphics::TextureResource& kernel = *state.metaball.getResource();
    for (std::size_t index = canvas.metaballBegin; index < canvas.metaballEnd; ++index) {
        const MetaballDraw& draw = state.metaballs[index];
        beginOffscreenPass(state.fields[draw.field], math::Color::transparent());
        sg_apply_pipeline(state.getPipeline(Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Additive), graphics::PassTarget::Offscreen));
        applyUniforms(matrix);

        sg_bindings bindings{};
        bindings.vertex_buffers[0] = state.quad;
        bindings.vertex_buffers[1] = state.instanceBuffer;
        bindings.vertex_buffer_offsets[1] = static_cast<int>((splatBase + draw.first) * sizeof(GpuInstance));
        bindings.views[VIEW_sprite_sprite_texture] = kernel.view;
        bindings.samplers[SMP_sprite_sprite_sampler] = kernel.sampler;
        sg_apply_bindings(&bindings);
        sg_draw(0, 4, static_cast<int>(draw.count));
        ++state.stats.drawCalls;
        sg_end_pass();
    }
}

void FrameSubmitter::renderCanvasOffscreen(Canvas& canvas) {
    if (!canvas.isComposited()) {
        if (canvas.kind == Canvas::Kind::Target) {
            const math::Rect rect = getPassRect(canvas);
            beginOffscreenPass(canvas.target, canvas.options.clear.value_or(math::Color::transparent()));
            sg_apply_viewportf(rect.x, rect.y, rect.width, rect.height, true);
            drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, rect);
            sg_end_pass();
        }
        return;
    }

    const LitTargets& targets = state.litTargets[canvas.litIndex];
    const math::Rect full{0.0F, 0.0F, static_cast<float>(targets.scene.getWidth()), static_cast<float>(targets.scene.getHeight())};
    const math::Color clear = canvas.options.clear.value_or(canvas.kind == Canvas::Kind::Target ? math::Color::transparent() : state.clearColor);
    if (canvas.isLit()) {
        beginLitPass(targets, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::LitScene, full);
        sg_end_pass();
        renderLights(canvas, targets);
    } else {
        beginOffscreenPass(targets.scene, clear);
        drawCommands(canvas, canvas.sceneBegin, canvas.sceneEnd, graphics::PassTarget::Offscreen, full);
        sg_end_pass();
    }
    renderPostChain(canvas, targets);

    // A render target canvas takes its finished image the way a world canvas reaches the screen.
    if (canvas.kind == Canvas::Kind::Target) {
        const math::Rect rect = getPassRect(canvas);
        beginOffscreenPass(canvas.target, clear);
        sg_apply_viewportf(rect.x, rect.y, rect.width, rect.height, true);
        drawFinal(canvas, graphics::PassTarget::Offscreen);
        sg_end_pass();
    }
}

void FrameSubmitter::renderLights(const Canvas& canvas, const LitTargets& targets) {
    beginOffscreenPass(targets.light, *canvas.options.ambientLight);
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    const math::Rect bounds = canvas.getWorldBounds();
    const sg_sampler nearest = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Nearest});

    for (std::size_t index = canvas.lightBegin; index < canvas.lightEnd; ++index) {
        const LightDraw& draw = state.lights[index];
        const lighting2d::Light& light = draw.light;
        const bool directional = light.type == lighting2d::Light::Type::Directional;
        sg_apply_pipeline(state.getPipeline(Program::Light, static_cast<std::uint8_t>(light.blend), graphics::PassTarget::LightMap));

        const graphics::TextureResource& shape = light.texture.isValid() ? *light.texture.getResource() : *state.light.getResource();
        sg_bindings bindings{};
        bindings.vertex_buffers[0] = state.quad;
        bindings.views[VIEW_light_shape_texture] = shape.view;
        bindings.views[VIEW_light_surface_texture] = targets.surface.getTexture().getResource()->view;
        bindings.views[VIEW_light_info_texture] = targets.info.getTexture().getResource()->view;
        bindings.views[VIEW_light_shadow_texture] = state.shadowView;
        bindings.samplers[SMP_light_shape_sampler] = shape.sampler;
        bindings.samplers[SMP_light_surface_sampler] = nearest;
        bindings.samplers[SMP_light_shadow_sampler] = nearest;
        sg_apply_bindings(&bindings);

        // Directional lights cover the view of the canvas, and the others a quad of their reach turned with them.
        light_light_vs_params_t vertex{};
        std::copy(matrix.begin(), matrix.end(), vertex.view_projection);
        const math::Vec2 center = directional ? bounds.getCenter() : light.position;
        const math::Vec2 half = directional ? bounds.getSize() * 0.5F : light.scale * light.radius;
        vertex.area[0] = center.x;
        vertex.area[1] = center.y;
        vertex.area[2] = half.x;
        vertex.area[3] = half.y;
        vertex.placement[0] = directional ? 0.0F : light.rotation;
        sg_apply_uniforms(UB_light_light_vs_params, SG_RANGE(vertex));

        const math::Vec2 direction = math::Vec2::fromAngle(light.rotation);
        const float energy = light.intensity * light.color.a;
        const float outer = std::cos(light.outerAngle * 0.5F);
        const int samples = light.shadowFilter == lighting2d::Light::ShadowFilter::Pcf13 ? 13 : (light.shadowFilter == lighting2d::Light::ShadowFilter::Pcf5 ? 5 : 1);
        const light_light_fs_params_t fragment{
            .color = {light.color.r * energy, light.color.g * energy, light.color.b * energy, static_cast<float>(light.blend)},
            .shape = {static_cast<float>(light.type), lighting2d::ShadowMap::getRange(light), light.height, static_cast<float>(draw.shadowRow)},
            .cone = {direction.x, direction.y, std::max(std::cos(light.innerAngle * 0.5F), outer + lighting2d::Light::kConeEdge), outer},
            .origin = {light.position.x, light.position.y, static_cast<float>(light.itemMask), 0.0F},
            .range = {static_cast<float>(std::clamp(light.layerMin, lighting2d::Light::kLowestLayer, lighting2d::Light::kHighestLayer)), static_cast<float>(std::clamp(light.layerMax, lighting2d::Light::kLowestLayer, lighting2d::Light::kHighestLayer)), static_cast<float>(samples), 1.0F + light.shadowSmoothness},
            .shadow_color = {light.shadowColor.r, light.shadowColor.g, light.shadowColor.b, light.shadowColor.a},
            .shadow_map = {static_cast<float>(lighting2d::ShadowMap::kResolution), lighting2d::ShadowMap::getBias(light, draw.axis), 0.0F, 0.0F},
            .shadow_axis = {draw.axis.acrossStart, draw.axis.acrossSpan, draw.axis.alongStart, draw.axis.alongSpan},
        };
        sg_apply_uniforms(UB_light_light_fs_params, SG_RANGE(fragment));
        sg_draw(0, 4, 1);
        ++state.stats.drawCalls;
    }
    sg_end_pass();
}

// The composite writes into the first post target, and every material but the last draws the image before it into the other one, which leaves the input of the last material in postImage.
void FrameSubmitter::renderPostChain(Canvas& canvas, const LitTargets& targets) {
    if (!canvas.hasPostMaterials()) {
        return;
    }

    beginOffscreenPass(targets.post[0], math::Color::transparent());
    composite(canvas, graphics::PassTarget::Offscreen);
    sg_end_pass();

    std::size_t current = 0;
    const std::size_t count = canvas.options.postProcess->materials.size();
    for (std::size_t index = 0; index + 1 < count; ++index) {
        beginOffscreenPass(targets.post[1 - current], math::Color::transparent());
        drawPostMaterial(canvas.postShade + index, targets.post[current].getTexture(), graphics::PassTarget::Offscreen);
        sg_end_pass();
        current = 1 - current;
    }
    canvas.postImage = current;
}

void FrameSubmitter::renderCapture(std::size_t index) {
    const Capture& capture = state.captures[index];
    beginOffscreenPass(capture.target, capture.clear);
    drawCanvases(index + 1, graphics::PassTarget::Offscreen);
    sg_end_pass();
}

// The frame of an opaque window clears its alpha to 1 and never writes it, whatever the clear color and the blends of the app.
void FrameSubmitter::renderSwapchain() {
    const math::Color& clear = state.clearColor;
    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {clear.r, clear.g, clear.b, target.transparent ? clear.a : 1.0F};
    pass.swapchain = target.swapchain;
    pass.label = "haylen-swapchain-pass";
    sg_begin_pass(&pass);
    ++state.stats.passes;

    drawCanvases(0, target.transparent ? graphics::PassTarget::TransparentSwapchain : graphics::PassTarget::Swapchain);
    sg_end_pass();
    sg_commit();
}

// Draws the world and screen canvases of a destination, the screen or a capture, by their order and then in the order they began.
void FrameSubmitter::drawCanvases(std::size_t capture, graphics::PassTarget pass) {
    std::vector<const Canvas*> ordered;
    for (const Canvas& canvas : state.canvases) {
        if (canvas.kind != Canvas::Kind::Target && canvas.capture == capture) {
            ordered.push_back(&canvas);
        }
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const Canvas* lhs, const Canvas* rhs) { return lhs->options.order < rhs->options.order; });

    for (const Canvas* canvas : ordered) {
        const math::Rect rect = getPassRect(*canvas);
        sg_apply_viewportf(rect.x, rect.y, rect.width, rect.height, true);
        if (canvas->isComposited()) {
            drawFinal(*canvas, pass);
            continue;
        }
        drawCommands(*canvas, canvas->sceneBegin, canvas->sceneEnd, pass, rect);
    }
}

void FrameSubmitter::drawFinal(const Canvas& canvas, graphics::PassTarget pass) {
    if (!canvas.hasPostMaterials()) {
        composite(canvas, pass);
        return;
    }
    const LitTargets& targets = state.litTargets[canvas.litIndex];
    drawPostMaterial(canvas.postShade + canvas.options.postProcess->materials.size() - 1, targets.post[canvas.postImage].getTexture(), pass);
}

void FrameSubmitter::beginOffscreenPass(const graphics::RenderTarget& renderTarget, math::Color clear) {
    sg_pass pass{};
    pass.action.colors[0].load_action = SG_LOADACTION_CLEAR;
    pass.action.colors[0].clear_value = {clear.r, clear.g, clear.b, clear.a};
    pass.attachments.colors[0] = renderTarget.getResource()->attachment;
    pass.label = "haylen-offscreen-pass";
    sg_begin_pass(&pass);
    ++state.stats.passes;
}

// The info image starts with the light mask 1 and the layer 0 of draws that set neither, so lights reach the background like such a draw.
void FrameSubmitter::beginLitPass(const LitTargets& targets, math::Color clear) {
    sg_pass pass{};
    const std::array<const graphics::RenderTarget*, 4> images{&targets.scene, &targets.emission, &targets.surface, &targets.info};
    const std::array<sg_color, 4> clears{sg_color{clear.r, clear.g, clear.b, clear.a}, sg_color{0.0F, 0.0F, 0.0F, 0.0F}, sg_color{0.5F, 0.5F, 0.0F, 0.0F}, sg_color{1.0F / 255.0F, 128.0F / 255.0F, 0.0F, 0.0F}};
    for (std::size_t index = 0; index < images.size(); ++index) {
        pass.action.colors[index].load_action = SG_LOADACTION_CLEAR;
        pass.action.colors[index].clear_value = clears[index];
        pass.attachments.colors[index] = images[index]->getResource()->attachment;
    }
    pass.label = "haylen-lit-scene-pass";
    sg_begin_pass(&pass);
    ++state.stats.passes;
}

void FrameSubmitter::composite(const Canvas& canvas, graphics::PassTarget pass) {
    const LitTargets& targets = state.litTargets[canvas.litIndex];
    const PostProcess post = canvas.options.postProcess.value_or(PostProcess{});
    const bool lit = canvas.isLit();

    sg_apply_pipeline(state.getPipeline(Program::Composite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), pass));
    sg_bindings bindings{};
    bindings.views[VIEW_composite_scene_texture] = targets.scene.getTexture().getResource()->view;
    bindings.views[VIEW_composite_light_texture] = (lit ? targets.light : targets.scene).getTexture().getResource()->view;
    bindings.views[VIEW_composite_emission_texture] = (lit ? targets.emission : targets.scene).getTexture().getResource()->view;
    bindings.samplers[SMP_composite_composite_sampler] = state.device.getState().getSampler({.filter = graphics::Texture::Filter::Linear});
    sg_apply_bindings(&bindings);

    const composite_composite_fs_params_t params{
        .tint = {post.tint.r, post.tint.g, post.tint.b, post.tint.a},
        .fade = {post.fade.r, post.fade.g, post.fade.b, post.fade.a},
        .grading = {post.saturation, post.brightness, post.contrast, 0.0F},
        .vignette = {post.vignetteStrength, post.vignetteRadius, post.vignetteSoftness, 0.0F},
        .flags = {lit ? 1.0F : 0.0F, sg_query_features().origin_top_left ? 0.0F : 1.0F, 0.0F, 0.0F},
    };
    sg_apply_uniforms(UB_composite_composite_fs_params, SG_RANGE(params));
    sg_draw(0, 3, 1);
    ++state.stats.drawCalls;
}

// A post-processing material draws the image as one sprite over the unit square, which covers the viewport of the pass.
void FrameSubmitter::drawPostMaterial(std::size_t shadeIndex, const graphics::Texture& image, graphics::PassTarget pass) {
    const Shade& shade = state.shades[shadeIndex];
    const graphics::TextureResource& resource = *image.getResource();
    sg_apply_pipeline(state.getMaterialPipeline(*shade.material, Program::Sprite, static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque), pass));
    applyUniforms(projection(math::Transform2D::identity(), {1.0F, 1.0F}));
    applyShade(shade, pass, Program::Sprite);

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = state.quad;
    bindings.vertex_buffers[1] = state.screen;
    bindings.vertex_buffer_offsets[1] = resource.flipped ? static_cast<int>(sizeof(GpuInstance)) : 0;
    bindMaterial(bindings, shade, resource, pass, Program::Sprite);
    sg_apply_bindings(&bindings);
    sg_draw(0, 4, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::applyClip(const Canvas& canvas, std::uint16_t clip, const math::Rect& passRect) {
    if (clip == 0) {
        sg_apply_scissor_rectf(passRect.x, passRect.y, passRect.width, passRect.height, true);
        return;
    }

    const math::Rect& area = state.clips[clip - 1U];
    const math::Vec2 scale = passRect.getSize() / canvas.viewSize;
    const std::array<math::Vec2, 4> corners{canvas.view.apply(area.getMin()), canvas.view.apply(math::Vec2{area.getRight(), area.getTop()}), canvas.view.apply(area.getMax()), canvas.view.apply(math::Vec2{area.getLeft(), area.getBottom()})};
    const math::Rect pixels = math::Geometry::bounds(corners);
    const math::Rect scissor = math::Rect{passRect.x + pixels.x * scale.x, passRect.y + pixels.y * scale.y, pixels.width * scale.x, pixels.height * scale.y}.intersection(passRect);
    sg_apply_scissor_rectf(scissor.x, scissor.y, std::max(0.0F, scissor.width), std::max(0.0F, scissor.height), true);
}

void FrameSubmitter::drawCommands(const Canvas& canvas, std::size_t begin, std::size_t end, graphics::PassTarget pass, const math::Rect& passRect) {
    const Matrix matrix = projection(canvas.view, canvas.viewSize);
    sg_pipeline currentPipeline{};
    graphics::TextureResource* currentTexture = nullptr;
    int currentClip = -1;
    std::uint32_t currentShade = 0;
    math::Vec2 currentOffset{};

    for (std::size_t index = begin; index < end; ++index) {
        const Command& command = state.commands[index];
        const Shade& shade = state.shades[command.shade];
        if (command.program == Program::ImageBlend || command.program == Program::Metaball) {
            // These apply their own pipeline, uniforms and clip, so the next command applies them again.
            sg_apply_pipeline(state.getPipeline(command.program, static_cast<std::uint8_t>(command.blend), pass));
            applyClip(canvas, command.clip, passRect);
            applyShade(shade, pass, command.program);
            if (command.program == Program::ImageBlend) {
                drawImageBlend(command, matrix);
            } else {
                drawMetaball(command, matrix);
            }
            currentPipeline = {};
            currentClip = -1;
            continue;
        }

        const auto blend = static_cast<std::uint8_t>(command.blend);
        const sg_pipeline pipeline = shade.material != nullptr ? state.getMaterialPipeline(*shade.material, command.program, blend, pass) : state.getPipeline(command.program, blend, pass);
        const bool fresh = pipeline.id != currentPipeline.id;
        if (fresh) {
            sg_apply_pipeline(pipeline);
            currentPipeline = pipeline;
            currentClip = -1;
        }
        if (fresh || command.offset != currentOffset) {
            applyUniforms(translated(matrix, command.offset));
            currentOffset = command.offset;
        }
        if (fresh || command.shade != currentShade) {
            applyShade(shade, pass, command.program);
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

        sg_bindings bindings{};
        if (command.program == Program::Mesh) {
            bindings.vertex_buffers[0] = state.vertexBuffer;
            bindings.index_buffer = state.indexBuffer;
        } else {
            bindings.vertex_buffers[0] = state.quad;
            bindings.vertex_buffers[1] = command.batch != nullptr ? command.batch->buffer : state.instanceBuffer;
            bindings.vertex_buffer_offsets[1] = static_cast<int>(command.first * sizeof(GpuInstance));
        }
        if (shade.material != nullptr) {
            bindMaterial(bindings, shade, *command.texture, pass, command.program);
        } else {
            bindings.views[VIEW_sprite_sprite_texture] = command.texture->view;
            bindings.samplers[SMP_sprite_sprite_sampler] = command.texture->sampler;
            if (pass == graphics::PassTarget::LitScene && command.program == Program::Sprite) {
                bindings.views[VIEW_sprite_lit_normal_texture] = (shade.normalMap != nullptr ? shade.normalMap : state.white.getResource().get())->view;
            }
        }
        sg_apply_bindings(&bindings);

        if (command.program == Program::Mesh) {
            sg_draw(static_cast<int>(command.first), static_cast<int>(command.count), 1);
        } else {
            sg_draw(0, 4, static_cast<int>(command.count));
        }
        ++state.stats.drawCalls;
    }
}

void FrameSubmitter::drawImageBlend(const Command& command, const Matrix& matrix) {
    const ImageBlend& blend = state.blends[command.first];
    const graphics::TextureResource& from = *blend.from.getResource();
    const graphics::TextureResource& to = *blend.to.getResource();

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = state.quad;
    bindings.views[VIEW_blend_from_texture] = from.view;
    bindings.views[VIEW_blend_to_texture] = to.view;
    bindings.samplers[SMP_blend_blend_sampler] = from.sampler;
    sg_apply_bindings(&bindings);

    blend_blend_vs_params_t vertex{};
    std::copy(matrix.begin(), matrix.end(), vertex.view_projection);
    vertex.area[0] = blend.area.x;
    vertex.area[1] = blend.area.y;
    vertex.area[2] = blend.area.width;
    vertex.area[3] = blend.area.height;
    sg_apply_uniforms(UB_blend_blend_vs_params, SG_RANGE(vertex));

    const float flips = (from.flipped ? 1.0F : 0.0F) + (to.flipped ? 2.0F : 0.0F);
    const blend_blend_fs_params_t fragment{
        .settings = {static_cast<float>(blend.pattern), blend.progress, blend.reversed ? 1.0F : 0.0F, blend.angle},
        .shape = {blend.center.x, blend.center.y, blend.cellSize, blend.blockSize},
        .color = {blend.color.r, blend.color.g, blend.color.b, blend.color.a},
        .image = {static_cast<float>(from.width), static_cast<float>(from.height), blend.area.height > 0.0F ? blend.area.width / blend.area.height : 1.0F, flips},
    };
    sg_apply_uniforms(UB_blend_blend_fs_params, SG_RANGE(fragment));
    sg_draw(0, 4, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::drawMetaball(const Command& command, const Matrix& matrix) {
    const MetaballDraw& draw = state.metaballs[command.first];
    const graphics::TextureResource& field = *state.fields[draw.field].getTexture().getResource();

    sg_bindings bindings{};
    bindings.vertex_buffers[0] = state.quad;
    bindings.views[VIEW_metaball_field_texture] = field.view;
    bindings.samplers[SMP_metaball_field_sampler] = field.sampler;
    sg_apply_bindings(&bindings);

    metaball_metaball_vs_params_t vertex{};
    std::copy(matrix.begin(), matrix.end(), vertex.view_projection);
    vertex.area[0] = draw.area.x;
    vertex.area[1] = draw.area.y;
    vertex.area[2] = draw.area.width;
    vertex.area[3] = draw.area.height;
    vertex.field[0] = field.flipped ? 1.0F : 0.0F;
    sg_apply_uniforms(UB_metaball_metaball_vs_params, SG_RANGE(vertex));

    const Renderer::MetaballStyle& style = draw.style;
    const metaball_metaball_fs_params_t fragment{
        .fill = {style.color.r, style.color.g, style.color.b, style.color.a},
        .outline_color = {style.outlineColor.r, style.outlineColor.g, style.outlineColor.b, style.outlineColor.a},
        .levels = {style.threshold, style.outlineWidth, 0.0F, 0.0F},
    };
    sg_apply_uniforms(UB_metaball_metaball_fs_params, SG_RANGE(fragment));
    sg_draw(0, 4, 1);
    ++state.stats.drawCalls;
}

void FrameSubmitter::applyUniforms(const Matrix& matrix) {
    sprite_haylen_vs_params_t params{};
    std::copy(matrix.begin(), matrix.end(), params.view_projection);
    sg_apply_uniforms(UB_sprite_haylen_vs_params, SG_RANGE(params));
}

void FrameSubmitter::applyShade(const Shade& shade, graphics::PassTarget pass, Program program) {
    if (pass == graphics::PassTarget::LitScene) {
        const sprite_lit_haylen_lit_params_t lit{
            .haylen_surface = {shade.specular, shade.emission, shade.unshaded ? 1.0F : 0.0F, 0.0F},
            .haylen_info = {static_cast<float>(shade.lightMask) / 255.0F, static_cast<float>(shade.layer + 128) / 255.0F, shade.normalMap != nullptr ? shade.shininess / 255.0F : 0.0F, 0.0F},
        };
        sg_apply_uniforms(UB_sprite_lit_haylen_lit_params, SG_RANGE(lit));
    }
    if (shade.material == nullptr) {
        return;
    }

    // Each uniform block of the material shader that the program declares takes its part of the copied values.
    const graphics::ShaderResource::Program& made = state.getMaterialProgram(*shade.material, program, pass);
    std::size_t offset = shade.uniformBegin;
    for (const graphics::Shader::Block& block : shade.material->shader.getBlocks()) {
        if (std::find(made.blocks.begin(), made.blocks.end(), block.slot) != made.blocks.end()) {
            sg_apply_uniforms(block.slot, {.ptr = state.uniformBytes.data() + offset, .size = block.size});
        }
        offset += block.size;
    }
}

void FrameSubmitter::bindMaterial(sg_bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass, Program program) {
    const graphics::ShaderResource::Program& made = state.getMaterialProgram(*shade.material, program, pass);
    const std::vector<graphics::Shader::TextureSlot>& textures = shade.material->shader.getTextures();
    // clang-format off
    const auto textureAt = [&](int slot) -> const graphics::TextureResource& {
        for (std::size_t index = 0; index < textures.size(); ++index) {
            if (textures[index].slot == slot) {
                return *state.shadeTextures[shade.textureBegin + index];
            }
        }
        return slot == 0 ? texture : *state.white.getResource();
    };
    // clang-format on

    for (const int slot : made.views) {
        bindings.views[static_cast<std::size_t>(slot)] = textureAt(slot).view;
    }
    for (const auto& [sampler, view] : made.samplers) {
        bindings.samplers[static_cast<std::size_t>(sampler)] = textureAt(view).sampler;
    }
}

} // namespace haylen::graphics2d
