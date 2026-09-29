#include "2d/graphics/RendererState.hpp"

#include <algorithm>
#include <stdexcept>

#include "2d/graphics/MaterialResource.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/Gpu.hpp"
#include "graphics/ShaderResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/graphics/Device.hpp"
#include "shaders/mesh.glsl.h"
#include "shaders/sprite.glsl.h"

namespace haylen::graphics2d {

Canvas& RendererState::getCanvas() {
    if (!canvasOpen) {
        throw std::logic_error("No canvas is active. Call beginWorld, beginScreen or beginTarget before drawing.");
    }
    return canvases.back();
}

bool RendererState::accepts(const DrawOrder& order) {
    return (order.visibility & getCanvas().options.visibilityMask) != 0;
}

void RendererState::retain(const graphics::Texture& texture) {
    if (retainedSet.insert(texture.getResource().get()).second) {
        retained.push_back(texture.getResource());
    }
}

void RendererState::openCanvas(Canvas next) {
    closeCanvas();
    next.itemBegin = items.size();
    next.itemEnd = items.size();
    next.lightBegin = lights.size();
    next.lightEnd = lights.size();
    next.segmentBegin = segments.size();
    next.segmentEnd = segments.size();
    next.metaballBegin = metaballs.size();
    next.metaballEnd = metaballs.size();
    if (captureOpen && next.kind != Canvas::Kind::Target) {
        next.capture = captures.size();
    }
    canvases.push_back(std::move(next));
    clipStack.clear();
    layerOffsets.clear();
    layerOffset = 0;
    canvasOpen = true;
    ++stats.canvases;
}

void RendererState::closeCanvas() {
    if (!canvasOpen) {
        return;
    }
    Canvas& canvas = canvases.back();
    canvas.itemEnd = items.size();
    canvas.lightEnd = lights.size();
    canvas.segmentEnd = segments.size();
    canvas.metaballEnd = metaballs.size();
    canvasOpen = false;
}

void RendererState::closeCapture() {
    if (captureOpen) {
        closeCanvas();
        captures.back().canvasEnd = canvases.size();
        captureOpen = false;
    }
}

void RendererState::resetFrame() noexcept {
    instances.clear();
    upload.clear();
    splats.clear();
    vertices.clear();
    indices.clear();
    uploadIndices.clear();
    blends.clear();
    items.clear();
    commands.clear();
    canvases.clear();
    captures.clear();
    clips.clear();
    clipStack.clear();
    layerOffsets.clear();
    shades.clear();
    uniformBytes.clear();
    shadeTextures.clear();
    lights.clear();
    segments.clear();
    metaballs.clear();
    retained.clear();
    retainedBatches.clear();
    retainedMaterials.clear();
    retainedSet.clear();
    sequence = 0;
    layerOffset = 0;
    canvasOpen = false;
    captureOpen = false;
}

std::uint32_t RendererState::getShade(const DrawOrder& order) {
    if (!(order.specular >= 0.0F && order.emission >= 0.0F)) {
        throw std::invalid_argument("A draw needs a specular strength and an emission of zero or more.");
    }
    if (!(order.shininess >= 1.0F && order.shininess <= 255.0F)) {
        throw std::invalid_argument("A draw shininess must be from 1 to 255.");
    }

    Shade shade;
    if (order.material.isValid()) {
        MaterialResource& material = *order.material.getResource();
        shade.material = &material;
        shade.revision = material.revision;
        shade.version = material.shader.getVersion();
    }
    // Unlit canvases ignore the lighting of draws, so it never splits their commands.
    if (getCanvas().isLit()) {
        shade.normalMap = order.normalMap.getResource().get();
        shade.specular = order.specular;
        shade.shininess = order.shininess;
        shade.emission = order.emission;
        shade.lightMask = order.lightMask;
        shade.layer = static_cast<std::int16_t>(std::clamp(order.layer + layerOffset, lighting2d::Light::kLowestLayer, lighting2d::Light::kHighestLayer));
        shade.unshaded = order.unshaded;
    }
    if (!shades.empty() && shades.back().matches(shade)) {
        return static_cast<std::uint32_t>(shades.size() - 1);
    }
    if (shade.normalMap != nullptr) {
        retain(order.normalMap);
    }
    return pushShade(shade, order.material);
}

std::uint32_t RendererState::addMaterialShade(const Material& material) {
    Shade shade;
    shade.material = material.getResource().get();
    shade.revision = shade.material->revision;
    shade.version = shade.material->shader.getVersion();
    return pushShade(shade, material);
}

// A new shade copies the values of its material, so later changes to the material leave the draws made so far alone.
std::uint32_t RendererState::pushShade(Shade shade, const Material& material) {
    if (shade.material != nullptr) {
        if (retainedSet.insert(shade.material).second) {
            retainedMaterials.push_back(material.getResource());
        }
        shade.uniformBegin = static_cast<std::uint32_t>(uniformBytes.size());
        shade.material->pack(uniformBytes);
        shade.textureBegin = static_cast<std::uint32_t>(shadeTextures.size());
        for (const graphics::Shader::TextureSlot& slot : shade.material->shader.getTextures()) {
            const graphics::Texture texture = shade.material->getTextureAt(slot.slot);
            const graphics::Texture& bound = texture.isValid() ? texture : white;
            retain(bound);
            shadeTextures.push_back(bound.getResource().get());
        }
    }
    shades.push_back(shade);
    return static_cast<std::uint32_t>(shades.size() - 1);
}

DrawItem& RendererState::addItem(Program program, const DrawOrder& order, graphics::TextureResource* texture, float standingY) {
    Canvas& current = getCanvas();
    if (order.material.isValid() && (program == Program::ImageBlend || program == Program::Metaball)) {
        throw std::invalid_argument("Image blends and metaballs do not take a material.");
    }

    DrawItem item{
        .key = DrawItem::makeKey(order, layerOffset, current.options.sort, standingY),
        .sequence = sequence++,
        .program = program,
        .blend = order.blend,
        .clip = clipStack.empty() ? std::uint16_t{0} : clipStack.back(),
        .shade = getShade(order),
        .texture = texture,
    };
    items.push_back(item);
    return items.back();
}

void RendererState::addInstances(Program program, const DrawOrder& order, const graphics::Texture& texture, std::span<const GpuInstance> data, float standingY) {
    retain(texture);
    DrawItem& item = addItem(program, order, texture.getResource().get(), standingY);
    item.first = static_cast<std::uint32_t>(instances.size());
    item.count = static_cast<std::uint32_t>(data.size());
    instances.insert(instances.end(), data.begin(), data.end());
    stats.sprites += data.size();
}

void RendererState::addMesh(const graphics::Texture& texture, std::span<const GpuVertex> meshVertices, std::span<const std::uint32_t> meshIndices, const DrawOrder& order) {
    if (meshVertices.empty() || meshIndices.empty()) {
        return;
    }

    const auto lowest = std::max_element(meshVertices.begin(), meshVertices.end(), [](const GpuVertex& lhs, const GpuVertex& rhs) { return lhs.position[1] < rhs.position[1]; });
    retain(texture);
    DrawItem& item = addItem(Program::Mesh, order, texture.getResource().get(), lowest->position[1]);
    const auto base = static_cast<std::uint32_t>(vertices.size());
    item.first = static_cast<std::uint32_t>(indices.size());
    item.count = static_cast<std::uint32_t>(meshIndices.size());
    vertices.insert(vertices.end(), meshVertices.begin(), meshVertices.end());
    for (const std::uint32_t index : meshIndices) {
        indices.push_back(base + index);
    }
}

std::uint32_t RendererState::pipelineKey(Program program, std::uint8_t blend, graphics::PassTarget target) noexcept {
    return static_cast<std::uint32_t>(program) | (static_cast<std::uint32_t>(blend) << 4U) | (static_cast<std::uint32_t>(target) << 8U);
}

sg_blend_state RendererState::blendState(graphics::BlendMode::Type mode) noexcept {
    using Mode = graphics::BlendMode::Type;
    switch (mode) {
    case Mode::Alpha:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    case Mode::Additive:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE};
    case Mode::Multiply:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_DST_COLOR, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    case Mode::Screen:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_ONE, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_COLOR, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    case Mode::Premultiplied:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_ONE, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA, .src_factor_alpha = SG_BLENDFACTOR_ONE, .dst_factor_alpha = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    case Mode::Opaque:
        return {};
    }
    return {};
}

sg_blend_state RendererState::lightBlend(std::uint8_t blend) noexcept {
    switch (static_cast<lighting2d::Light::Blend>(blend)) {
    case lighting2d::Light::Blend::Add:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_ONE, .dst_factor_rgb = SG_BLENDFACTOR_ONE};
    case lighting2d::Light::Blend::Subtract:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_ONE, .dst_factor_rgb = SG_BLENDFACTOR_ONE, .op_rgb = SG_BLENDOP_REVERSE_SUBTRACT};
    case lighting2d::Light::Blend::Mix:
        return {.enabled = true, .src_factor_rgb = SG_BLENDFACTOR_SRC_ALPHA, .dst_factor_rgb = SG_BLENDFACTOR_ONE_MINUS_SRC_ALPHA};
    }
    return {};
}

void RendererState::describeLayout(sg_pipeline_desc& desc, Program program) const {
    // The vertex layouts below read these records at fixed offsets.
    static_assert(sizeof(GpuInstance) == 48);
    static_assert(sizeof(GpuVertex) == 20);

    switch (program) {
    case Program::Mesh:
        desc.layout.attrs[ATTR_mesh_mesh_position] = {.buffer_index = 0, .offset = 0, .format = SG_VERTEXFORMAT_FLOAT2};
        desc.layout.attrs[ATTR_mesh_mesh_texcoord] = {.buffer_index = 0, .offset = 8, .format = SG_VERTEXFORMAT_FLOAT2};
        desc.layout.attrs[ATTR_mesh_mesh_color0] = {.buffer_index = 0, .offset = 16, .format = SG_VERTEXFORMAT_UBYTE4N};
        desc.layout.buffers[0].stride = sizeof(GpuVertex);
        desc.index_type = SG_INDEXTYPE_UINT32;
        desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLES;
        return;
    case Program::ImageBlend:
    case Program::Light:
    case Program::Metaball:
        // Each of these draws one quad whose corner is the only attribute.
        desc.layout.buffers[0].stride = 8;
        desc.layout.attrs[0] = {.buffer_index = 0, .offset = 0, .format = SG_VERTEXFORMAT_FLOAT2};
        desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
        return;
    case Program::Composite:
        return;
    case Program::Sprite:
    case Program::Text:
        break;
    }

    desc.layout.buffers[0].stride = 8;
    desc.layout.buffers[1].stride = sizeof(GpuInstance);
    desc.layout.buffers[1].step_func = SG_VERTEXSTEP_PER_INSTANCE;
    desc.layout.attrs[ATTR_sprite_sprite_corner] = {.buffer_index = 0, .offset = 0, .format = SG_VERTEXFORMAT_FLOAT2};
    desc.layout.attrs[ATTR_sprite_sprite_instance_position] = {.buffer_index = 1, .offset = 0, .format = SG_VERTEXFORMAT_FLOAT2};
    desc.layout.attrs[ATTR_sprite_sprite_instance_size] = {.buffer_index = 1, .offset = 8, .format = SG_VERTEXFORMAT_FLOAT2};
    desc.layout.attrs[ATTR_sprite_sprite_instance_uv] = {.buffer_index = 1, .offset = 16, .format = SG_VERTEXFORMAT_USHORT4N};
    desc.layout.attrs[ATTR_sprite_sprite_instance_color] = {.buffer_index = 1, .offset = 24, .format = SG_VERTEXFORMAT_UBYTE4N};
    desc.layout.attrs[ATTR_sprite_sprite_instance_flash] = {.buffer_index = 1, .offset = 28, .format = SG_VERTEXFORMAT_UBYTE4N};
    desc.layout.attrs[ATTR_sprite_sprite_instance_rotation] = {.buffer_index = 1, .offset = 32, .format = SG_VERTEXFORMAT_FLOAT};
    desc.layout.attrs[ATTR_sprite_sprite_instance_parameters] = {.buffer_index = 1, .offset = 36, .format = SG_VERTEXFORMAT_UBYTE4N};
    desc.layout.attrs[ATTR_sprite_sprite_instance_pivot] = {.buffer_index = 1, .offset = 40, .format = SG_VERTEXFORMAT_FLOAT2};
    desc.primitive_type = SG_PRIMITIVETYPE_TRIANGLE_STRIP;
}

void RendererState::describeTargets(sg_pipeline_desc& desc, Program program, std::uint8_t blend, graphics::PassTarget target) const {
    using Mode = graphics::BlendMode::Type;
    const auto mode = static_cast<Mode>(blend);
    const sg_pixel_format offscreen = device.getState().offscreenFormat;
    if (target != graphics::PassTarget::Swapchain && target != graphics::PassTarget::TransparentSwapchain) {
        desc.depth.pixel_format = SG_PIXELFORMAT_NONE;
        desc.sample_count = 1;
    }

    switch (target) {
    case graphics::PassTarget::Swapchain:
        // The swapchain of an opaque window keeps the alpha of its clear, so no blend can let the desktop through.
        desc.colors[0].blend = program == Program::Composite ? sg_blend_state{} : blendState(mode);
        desc.colors[0].write_mask = SG_COLORMASK_RGB;
        return;
    case graphics::PassTarget::TransparentSwapchain:
        desc.colors[0].blend = program == Program::Composite ? sg_blend_state{} : blendState(mode);
        return;
    case graphics::PassTarget::Offscreen:
        desc.colors[0].pixel_format = offscreen;
        desc.colors[0].blend = program == Program::Composite ? sg_blend_state{} : blendState(mode);
        return;
    case graphics::PassTarget::LightMap:
        desc.colors[0].pixel_format = lightFormat;
        desc.colors[0].blend = lightBlend(blend);
        desc.colors[0].write_mask = SG_COLORMASK_RGB;
        return;
    case graphics::PassTarget::LitScene:
        break;
    }

    // The emission follows the blend of the draw, except that multiplying draws leave it alone, and only draws that cover what is below write the surface and the info.
    desc.color_count = 4;
    for (int index = 0; index < 4; ++index) {
        desc.colors[index].pixel_format = offscreen;
    }
    desc.colors[0].blend = blendState(mode);
    desc.colors[1].blend = blendState(mode);
    if (mode == Mode::Multiply) {
        desc.colors[1].write_mask = SG_COLORMASK_NONE;
    }
    const bool covers = mode == Mode::Alpha || mode == Mode::Premultiplied || mode == Mode::Opaque;
    for (int index = 2; index < 4; ++index) {
        desc.colors[index].blend = mode == Mode::Opaque ? sg_blend_state{} : blendState(Mode::Alpha);
        if (!covers) {
            desc.colors[index].write_mask = SG_COLORMASK_NONE;
        }
    }
}

sg_pipeline RendererState::getPipeline(Program program, std::uint8_t blend, graphics::PassTarget target) {
    const std::uint8_t used = program == Program::Composite ? static_cast<std::uint8_t>(graphics::BlendMode::Type::Opaque) : blend;
    const std::uint32_t key = pipelineKey(program, used, target);
    if (const auto found = pipelines.find(key); found != pipelines.end()) {
        return found->second;
    }

    sg_pipeline_desc desc{};
    describeLayout(desc, program);
    describeTargets(desc, program, used, target);
    const auto index = static_cast<std::size_t>(program);
    desc.shader = target == graphics::PassTarget::LitScene ? litShaders[index] : shaders[index];
    desc.label = "haylen-pipeline";

    const sg_pipeline created = sg_make_pipeline(&desc);
    pipelines.emplace(key, created);
    return created;
}

const char* RendererState::materialProgramName(Program program, graphics::PassTarget target) {
    const bool lit = target == graphics::PassTarget::LitScene;
    switch (program) {
    case Program::Sprite:
        return lit ? "sprite_lit" : "sprite";
    case Program::Text:
        return lit ? "text_lit" : "text";
    case Program::Mesh:
        return lit ? "mesh_lit" : "mesh";
    case Program::ImageBlend:
    case Program::Composite:
    case Program::Light:
    case Program::Metaball:
        break;
    }
    throw std::logic_error("Materials only replace the sprite, text and mesh programs.");
}

const graphics::ShaderResource::Program& RendererState::getMaterialProgram(MaterialResource& material, Program program, graphics::PassTarget target) {
    // The shader hands its GPU objects to the device graveyard once it changes or goes away.
    graphics::ShaderResource& shader = *material.shader.getResource();
    if (shader.graveyard.expired()) {
        shader.graveyard = device.getState().graveyard;
    }
    return shader.getProgram(materialProgramName(program, target));
}

sg_pipeline RendererState::getMaterialPipeline(MaterialResource& material, Program program, std::uint8_t blend, graphics::PassTarget target) {
    graphics::ShaderResource& shader = *material.shader.getResource();
    const std::uint32_t key = pipelineKey(program, blend, target);
    if (const auto found = shader.pipelines.find(key); found != shader.pipelines.end()) {
        return found->second;
    }

    sg_pipeline_desc desc{};
    describeLayout(desc, program);
    describeTargets(desc, program, blend, target);
    desc.shader = getMaterialProgram(material, program, target).shader;
    desc.label = "haylen-material-pipeline";

    const sg_pipeline created = sg_make_pipeline(&desc);
    shader.pipelines.emplace(key, created);
    return created;
}

void RendererState::ensureBuffer(sg_buffer& buffer, std::size_t& capacity, std::size_t needed, std::size_t elementSize, bool isIndexBuffer) {
    if (needed <= capacity) {
        return;
    }

    if (buffer.id != SG_INVALID_ID) {
        device.getState().graveyard->buryBuffer(buffer);
    }
    capacity = std::max<std::size_t>(needed + needed / 2, 1024);

    sg_buffer_desc desc{};
    desc.size = capacity * elementSize;
    desc.usage.write_transient = true;
    desc.usage.immutable = false;
    desc.usage.vertex_buffer = !isIndexBuffer;
    desc.usage.index_buffer = isIndexBuffer;
    desc.label = isIndexBuffer ? "haylen-index-buffer" : "haylen-vertex-buffer";
    buffer = graphics::Gpu::makeBuffer(desc);
}

} // namespace haylen::graphics2d
