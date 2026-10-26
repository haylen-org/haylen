#include "2d/graphics/RendererState.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "2d/graphics/MaterialResource.hpp"
#include "core/EmbeddedFiles.hpp"
#include "graphics/DeviceState.hpp"
#include "graphics/ShaderResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/graphics/Device.hpp"

namespace haylen::graphics2d {

debug::ObjectCounter& RendererState::bufferCounter = *new debug::ObjectCounter("RendererBuffers", debug::ObjectCounter::Kind::Native);

std::uint64_t RendererState::submissions = 0;
const std::array<RendererState::ProgramSource, RendererState::kProgramCount> RendererState::kPrograms{{
    {.package = 0, .program = "sprite", .lit = "sprite_lit"},
    {.package = 1, .program = "text", .lit = "text_lit"},
    {.package = 2, .program = "mesh", .lit = "mesh_lit"},
    {.package = 3, .program = "blend", .lit = "blend_lit"},
    {.package = 4, .program = "composite", .lit = ""},
    {.package = 5, .program = "light", .lit = ""},
    {.package = 6, .program = "metaball", .lit = "metaball_lit"},
    {.package = 7, .program = "recolor", .lit = "recolor_lit"},
    {.package = 4, .program = "filter", .lit = ""},
    {.package = 8, .program = "effect", .lit = "effect_lit"},
    {.package = 9, .program = "shape", .lit = "shape_lit"},
    {.package = 0, .program = "batch", .lit = "batch_lit"},
    {.package = 10, .program = "backdrop", .lit = ""},
}};

// The vertex layouts below read these records at fixed offsets.
static_assert(sizeof(GpuInstance) == 48 && sizeof(GpuVertex) == 20 && sizeof(LightInstance) == 144);

const std::array<RendererState::Input, 9> RendererState::kSpriteInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_position", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_size", .buffer = 1, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_uv", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::UShort4Normalized},
    {.name = "instance_color", .buffer = 1, .offset = 24, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_flash", .buffer = 1, .offset = 28, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_rotation", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::Float},
    {.name = "instance_parameters", .buffer = 1, .offset = 36, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_pivot", .buffer = 1, .offset = 40, .format = gpu::Device::VertexFormat::Float2},
}};

const std::array<RendererState::Input, 11> RendererState::kBatchInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_position", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_size", .buffer = 1, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_uv", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::UShort4Normalized},
    {.name = "instance_color", .buffer = 1, .offset = 24, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_flash", .buffer = 1, .offset = 28, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_rotation", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::Float},
    {.name = "instance_parameters", .buffer = 1, .offset = 36, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_pivot", .buffer = 1, .offset = 40, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_data", .buffer = 2, .offset = 0, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_texture", .buffer = 2, .offset = 16, .format = gpu::Device::VertexFormat::Float},
}};

const std::array<RendererState::Input, 13> RendererState::kRecolorInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_position", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_size", .buffer = 1, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_uv", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::UShort4Normalized},
    {.name = "instance_color", .buffer = 1, .offset = 24, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_flash", .buffer = 1, .offset = 28, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_rotation", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::Float},
    {.name = "instance_parameters", .buffer = 1, .offset = 36, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_pivot", .buffer = 1, .offset = 40, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_red", .buffer = 2, .offset = 0, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_green", .buffer = 2, .offset = 4, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_blue", .buffer = 2, .offset = 8, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_yellow", .buffer = 2, .offset = 12, .format = gpu::Device::VertexFormat::UByte4Normalized},
}};

const std::array<RendererState::Input, 15> RendererState::kEffectInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_position", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_size", .buffer = 1, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_uv", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::UShort4Normalized},
    {.name = "instance_color", .buffer = 1, .offset = 24, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_flash", .buffer = 1, .offset = 28, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_rotation", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::Float},
    {.name = "instance_parameters", .buffer = 1, .offset = 36, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "instance_pivot", .buffer = 1, .offset = 40, .format = gpu::Device::VertexFormat::Float2},
    {.name = "effect_dissolve_color", .buffer = 2, .offset = 0, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "effect_outline_color", .buffer = 2, .offset = 4, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "effect_glow_color", .buffer = 2, .offset = 8, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "effect_amounts", .buffer = 2, .offset = 12, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "effect_reach", .buffer = 2, .offset = 16, .format = gpu::Device::VertexFormat::Float2},
    {.name = "effect_widths", .buffer = 2, .offset = 24, .format = gpu::Device::VertexFormat::Float2},
}};

const std::array<RendererState::Input, 9> RendererState::kShapeInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "shape_center", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "shape_half_size", .buffer = 1, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "shape_rotation", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::Float},
    {.name = "shape_reach", .buffer = 1, .offset = 20, .format = gpu::Device::VertexFormat::Half2},
    {.name = "shape_color", .buffer = 1, .offset = 24, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "shape_border_color", .buffer = 1, .offset = 28, .format = gpu::Device::VertexFormat::UByte4Normalized},
    {.name = "shape_form", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::UShort4Normalized},
    {.name = "shape_radii", .buffer = 1, .offset = 40, .format = gpu::Device::VertexFormat::UShort4Normalized},
}};

const std::array<RendererState::Input, 10> RendererState::kLightInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "instance_area", .buffer = 1, .offset = 0, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_color", .buffer = 1, .offset = 16, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_shape", .buffer = 1, .offset = 32, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_cone", .buffer = 1, .offset = 48, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_origin", .buffer = 1, .offset = 64, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_range", .buffer = 1, .offset = 80, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_shadow_color", .buffer = 1, .offset = 96, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_shadow_map", .buffer = 1, .offset = 112, .format = gpu::Device::VertexFormat::Float4},
    {.name = "instance_shadow_axis", .buffer = 1, .offset = 128, .format = gpu::Device::VertexFormat::Float4},
}};

const std::array<RendererState::Input, 3> RendererState::kMeshInputs{{
    {.name = "position", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
    {.name = "texcoord", .buffer = 0, .offset = 8, .format = gpu::Device::VertexFormat::Float2},
    {.name = "color0", .buffer = 0, .offset = 16, .format = gpu::Device::VertexFormat::UByte4Normalized},
}};

const std::array<RendererState::Input, 1> RendererState::kQuadInputs{{
    {.name = "corner", .buffer = 0, .offset = 0, .format = gpu::Device::VertexFormat::Float2},
}};

Canvas& RendererState::getCanvas() {
    if (!canvasOpen) {
        throw std::logic_error("No canvas is active. Call \"beginWorld\", \"beginScreen\" or \"beginTarget\" before drawing.");
    }
    return canvases.back();
}

math::Rect RendererState::getPassRect(const Canvas& canvas) const {
    math::Rect destination = pixelRect;
    if (canvas.kind == Canvas::Kind::Target) {
        destination = {0.0F, 0.0F, canvas.target.getSize().x, canvas.target.getSize().y};
    } else if (canvas.capture != 0) {
        const math::Vec2 size = captures[canvas.capture - 1].target.getSize();
        destination = {0.0F, 0.0F, size.x, size.y};
    }
    // The edges land on whole pixels, where the GPU places the viewport, so canvases that split a destination share their edges without a gap.
    const math::Rect& part = canvas.frame;
    const math::Vec2 first{destination.x + part.x * destination.width, destination.y + part.y * destination.height};
    const math::Vec2 last = first + math::Vec2{part.width * destination.width, part.height * destination.height};
    return math::Rect::fromMinMax(math::Vec2::round(first), math::Vec2::round(last));
}

// The view takes canvas units to the units of the view size, which the pass stretches over its pixels.
float RendererState::getPixelsPerUnit() {
    const Canvas& canvas = getCanvas();
    const math::Vec2 pixels = getPassRect(canvas).getSize() / canvas.viewSize;
    const math::Transform2D& view = canvas.view;
    return std::max(std::hypot(view.a, view.b) * pixels.x, std::hypot(view.c, view.d) * pixels.y);
}

float RendererState::getPixelSize() {
    const Canvas& canvas = getCanvas();
    const math::Vec2 pixels = getPassRect(canvas).getSize() / canvas.viewSize;
    const math::Transform2D& view = canvas.view;
    return 1.0F / std::min(std::hypot(view.a, view.b) * pixels.x, std::hypot(view.c, view.d) * pixels.y);
}

std::optional<TextPainter::PixelGrid> RendererState::getPixelGrid() {
    const Canvas& canvas = getCanvas();
    const math::Transform2D& view = canvas.view;
    if (view.b != 0.0F || view.c != 0.0F) {
        return std::nullopt;
    }
    const math::Rect pass = getPassRect(canvas);
    const math::Vec2 pixels = pass.getSize() / canvas.viewSize;
    return TextPainter::PixelGrid{.scale = {view.a * pixels.x, view.d * pixels.y}, .offset = {view.tx * pixels.x + pass.x, view.ty * pixels.y + pass.y}};
}

// Distortion draws need the distortion map of a composited canvas, and other canvases skip them.
bool RendererState::accepts(const DrawOrder& order) {
    const Canvas& canvas = getCanvas();
    if (order.distortion > 0.0F && !canvas.isComposited()) {
        return false;
    }
    return (order.visibility & canvas.options.visibilityMask) != 0;
}

void RendererState::retain(const graphics::Texture& texture) {
    graphics::TextureResource& resource = *texture.getResource();
    if (resource.retainEpoch != epoch) {
        resource.retainEpoch = epoch;
        retained.push_back(texture.getResource());
    }
}

void RendererState::retain(const std::shared_ptr<StaticBatchResource>& batch) {
    if (batch->retainEpoch != epoch) {
        batch->retainEpoch = epoch;
        retainedBatches.push_back(batch);
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
    if (!openCaptures.empty() && next.kind != Canvas::Kind::Target) {
        next.capture = openCaptures.back() + 1;
    }
    if (!next.isComposited() && next.kind == Canvas::Kind::Target) {
        next.destination = next.target.getTexture().getResource().get();
    } else if (!next.isComposited() && next.capture != 0) {
        next.destination = captures[next.capture - 1].target.getTexture().getResource().get();
    }
    canvases.push_back(std::move(next));
    clipStack.clear();
    layerOffsets.clear();
    layerOffset = 0;
    canvasOpen = true;
    ++stats.canvases;
}

std::size_t RendererState::acquireGroupTarget(math::Vec2 size) {
    const int width = std::max(1, static_cast<int>(std::lround(size.x)));
    const int height = std::max(1, static_cast<int>(std::lround(size.y)));
    const std::size_t index = groupTargetsUsed++;
    if (groupTargets.size() <= index) {
        groupTargets.resize(index + 1);
    }
    graphics::RenderTarget& target = groupTargets[index];
    if (!target.isValid() || target.getWidth() != width || target.getHeight() != height) {
        target = device.createRenderTarget(width, height, {.filter = graphics::Texture::Filter::Linear});
    }
    return index;
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
    closeCanvas();
    captures[openCaptures.back()].canvasEnd = canvases.size();
    closedCaptures.push_back(openCaptures.back());
    openCaptures.pop_back();
}

void RendererState::resetFrame() noexcept {
    instances.clear();
    upload.clear();
    splats.clear();
    vertices.clear();
    indices.clear();
    uploadIndices.clear();
    blends.clear();
    groups.clear();
    savedClips.clear();
    savedLayerOffsets.clear();

    // Images past what this frame used go back to the device.
    groupTargets.resize(groupTargetsUsed);
    groupTargetsUsed = 0;
    items.clear();
    commands.clear();
    canvases.clear();
    captures.clear();
    openCaptures.clear();
    closedCaptures.clear();
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
    epoch = ++submissions;
    placements.clear();
    shadowCasts.clear();
    sequence = 0;
    layerOffset = 0;
    canvasOpen = false;
}

std::uint32_t RendererState::getShade(Program program, const DrawOrder& order, std::span<const graphics::Texture> textures) {
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
    if (program == Program::Recolor) {
        shade.partMask = order.partMask.getResource().get();
    }
    if (program == Program::Batch) {
        for (std::size_t index = 0; index < shade.batchTextures.size(); ++index) {
            shade.batchTextures[index] = textures[index + 1 < textures.size() ? index + 1 : 0].getResource().get();
        }
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
    if (shade.partMask != nullptr) {
        retain(order.partMask);
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
        if (shade.material->retainEpoch != epoch) {
            shade.material->retainEpoch = epoch;
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

void RendererState::requireReadable(const graphics::TextureResource* texture) {
    if (texture != nullptr && texture == getCanvas().destination) {
        throw std::invalid_argument("A draw cannot sample the render target that its canvas draws into.");
    }
}

DrawItem& RendererState::addItem(Program program, const DrawOrder& order, graphics::TextureResource* texture, float standingY, std::span<const graphics::Texture> textures) {
    Canvas& current = getCanvas();
    if (order.material.isValid() && (program == Program::ImageBlend || program == Program::Backdrop || program == Program::Metaball)) {
        throw std::invalid_argument("Image blends and metaballs do not take a material.");
    }
    if (graphics::BlendMode::readsBackdrop(order.blend) && program != Program::Backdrop) {
        throw std::invalid_argument(std::format("The blend mode \"{}\" mixes with what lies below, which only groups do. Draw inside \"beginGroup\" with that blend instead.", graphics::BlendMode::name(order.blend)));
    }
    if (program == Program::Backdrop && current.isLit()) {
        throw std::invalid_argument("A group that blends with what lies below draws in a canvas without ambient light.");
    }
    if (order.material.isValid() && program == Program::Recolor) {
        throw std::invalid_argument("A draw with a part mask does not take a material.");
    }
    if (order.material.isValid() && program == Program::Effect) {
        throw std::invalid_argument("A sprite with an effect does not take a material.");
    }
    if (program == Program::Batch && textures.size() > 1 && order.normalMap.isValid()) {
        throw std::invalid_argument("A sprite batch with several textures draws without a normal map.");
    }
    const bool distorts = order.distortion > 0.0F;
    if (distorts && program == Program::Batch) {
        throw std::invalid_argument("A sprite batch with several textures or sprite data draws without a distortion.");
    }
    if (distorts && program != Program::Sprite && program != Program::Mesh && program != Program::Shape) {
        throw std::invalid_argument("Only sprites, sprite batches, nine-slices, shapes and meshes draw as distortion.");
    }
    requireReadable(texture);
    for (const graphics::Texture& batchTexture : textures) {
        requireReadable(batchTexture.getResource().get());
    }
    if (program == Program::Recolor) {
        requireReadable(order.partMask.getResource().get());
    }
    if (order.material.isValid() && current.destination != nullptr) {
        const MaterialResource& material = *order.material.getResource();
        for (const graphics::Shader::TextureSlot& slot : material.shader.getTextures()) {
            requireReadable(material.getTextureAt(slot.slot).getResource().get());
        }
    }

    // The coverage of distortion draws adds up in the distortion map, whatever their blend.
    DrawItem item{
        .key = DrawItem::makeKey(order, layerOffset, current.options.sort, standingY),
        .sortId = order.sortId,
        .sequence = sequence++,
        .program = program,
        .blend = distorts ? graphics::BlendMode::Type::Additive : order.blend,
        .clip = clipStack.empty() ? 0U : clipStack.back(),
        .shade = getShade(program, order, textures),
        .texture = texture,
        .distortion = distorts,
    };
    current.distorted = current.distorted || distorts;
    current.backdrop = current.backdrop || program == Program::Backdrop;
    items.push_back(item);
    return items.back();
}

void RendererState::addInstances(Program program, const DrawOrder& order, const graphics::Texture& texture, std::span<const GpuInstance> data, float standingY) {
    retain(texture);
    DrawItem& item = addItem(program, order, texture.getResource().get(), standingY);
    item.first = static_cast<std::uint32_t>(instances.size());
    item.count = static_cast<std::uint32_t>(data.size());
    instances.insert(instances.end(), data.begin(), data.end());
    if (item.distortion) {
        scaleCoverage(std::span(instances).subspan(item.first), program, order.distortion);
    }
    if (program != Program::Shape) {
        stats.sprites += program == Program::Recolor || program == Program::Effect ? data.size() / 2 : data.size();
    }
}

void RendererState::addShape(const Shape& shape, const DrawOrder& order) {
    const graphics::Texture& texture = shape.texture.isValid() ? shape.texture : white;
    const GpuInstance packed = GpuInstance::makeShape(shape, getPixelSize() + shape.softness * 0.5F, texture.getResource()->flipped);
    const math::Vec2 half = shape.bounds.getSize() * 0.5F;
    const float lowest = shape.bounds.getCenter().y + std::fabs(std::sin(shape.rotation)) * half.x + std::fabs(std::cos(shape.rotation)) * half.y;
    addInstances(Program::Shape, order, texture, std::span(&packed, 1), lowest);
}

void RendererState::scaleCoverage(std::span<GpuInstance> data, Program program, float distortion) noexcept {
    for (GpuInstance& instance : data) {
        if (program == Program::Shape) {
            instance.fadeShape(distortion);
        } else {
            instance.fade(distortion);
        }
    }
}

void RendererState::scaleCoverage(std::span<GpuVertex> data, float distortion) noexcept {
    for (GpuVertex& vertex : data) {
        const auto alpha = static_cast<float>(vertex.color >> 24U) * distortion;
        vertex.color = (vertex.color & 0x00FFFFFFU) | (static_cast<std::uint32_t>(std::lround(std::min(alpha, 255.0F))) << 24U);
    }
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
    if (item.distortion) {
        scaleCoverage(std::span(vertices).subspan(base), order.distortion);
    }
    for (const std::uint32_t index : meshIndices) {
        indices.push_back(base + index);
    }
}

std::uint32_t RendererState::pipelineKey(Program program, std::uint8_t blend, graphics::PassTarget target) noexcept {
    return static_cast<std::uint32_t>(program) | (static_cast<std::uint32_t>(blend) << 4U) | (static_cast<std::uint32_t>(target) << 8U);
}

bool RendererState::expectsPremultiplied(graphics::BlendMode::Type mode) noexcept {
    return mode == graphics::BlendMode::Type::Multiply || mode == graphics::BlendMode::Type::Screen;
}

gpu::Device::Blend RendererState::blendState(graphics::BlendMode::Type mode) noexcept {
    using Mode = graphics::BlendMode::Type;
    using Factor = gpu::Device::BlendFactor;
    switch (mode) {
    case Mode::Alpha:
        return {.enabled = true, .sourceColor = Factor::SourceAlpha, .destinationColor = Factor::OneMinusSourceAlpha, .sourceAlpha = Factor::One, .destinationAlpha = Factor::OneMinusSourceAlpha};
    case Mode::Additive:
        return {.enabled = true, .sourceColor = Factor::SourceAlpha, .destinationColor = Factor::One, .sourceAlpha = Factor::One, .destinationAlpha = Factor::One};
    case Mode::Multiply:
        return {.enabled = true, .sourceColor = Factor::DestinationColor, .destinationColor = Factor::OneMinusSourceAlpha, .sourceAlpha = Factor::One, .destinationAlpha = Factor::OneMinusSourceAlpha};
    case Mode::Screen:
        return {.enabled = true, .sourceColor = Factor::One, .destinationColor = Factor::OneMinusSourceColor, .sourceAlpha = Factor::One, .destinationAlpha = Factor::OneMinusSourceAlpha};
    case Mode::Premultiplied:
        return {.enabled = true, .sourceColor = Factor::One, .destinationColor = Factor::OneMinusSourceAlpha, .sourceAlpha = Factor::One, .destinationAlpha = Factor::OneMinusSourceAlpha};
    // The modes that mix with what lies below replace the pixels with what the backdrop program computed.
    case Mode::Opaque:
    case Mode::Overlay:
    case Mode::Darken:
    case Mode::Lighten:
    case Mode::ColorDodge:
    case Mode::ColorBurn:
    case Mode::HardLight:
    case Mode::SoftLight:
    case Mode::Difference:
    case Mode::Exclusion:
        return {};
    }
    return {};
}

gpu::Device::Blend RendererState::lightBlend(std::uint8_t blend) noexcept {
    using Factor = gpu::Device::BlendFactor;
    switch (static_cast<lighting2d::Light::Blend>(blend)) {
    case lighting2d::Light::Blend::Add:
        return {.enabled = true, .sourceColor = Factor::One, .destinationColor = Factor::One};
    case lighting2d::Light::Blend::Subtract:
        return {.enabled = true, .sourceColor = Factor::One, .destinationColor = Factor::One, .colorOperation = gpu::Device::BlendOperation::ReverseSubtract};
    case lighting2d::Light::Blend::Mix:
        return {.enabled = true, .sourceColor = Factor::SourceAlpha, .destinationColor = Factor::OneMinusSourceAlpha};
    case lighting2d::Light::Blend::Max:
        return {.enabled = true, .sourceColor = Factor::One, .destinationColor = Factor::One, .colorOperation = gpu::Device::BlendOperation::Maximum};
    }
    return {};
}

// A recolored sprite, a sprite with an effect and a sprite of a batch take two records of the instance stream, the sprite and then its part colors, its effect or its data, which two buffers read at the same instance.
RendererState::Layout RendererState::describeLayout(Program program) {
    constexpr int kCorner = 8;
    constexpr int kInstance = static_cast<int>(sizeof(GpuInstance));
    constexpr int kPaired = 2 * kInstance;
    Layout layout;
    switch (program) {
    case Program::Mesh:
        layout.buffers[0] = {.stride = static_cast<int>(sizeof(GpuVertex))};
        layout.bufferCount = 1;
        layout.inputs = kMeshInputs;
        layout.primitive = gpu::Device::Primitive::Triangles;
        layout.indexType = gpu::Device::IndexType::UInt32;
        return layout;
    case Program::ImageBlend:
    case Program::Backdrop:
    case Program::Metaball:
        layout.buffers[0] = {.stride = kCorner};
        layout.bufferCount = 1;
        layout.inputs = kQuadInputs;
        return layout;
    case Program::Light:
        layout.buffers[0] = {.stride = kCorner};
        layout.buffers[1] = {.stride = static_cast<int>(sizeof(LightInstance)), .perInstance = true};
        layout.bufferCount = 2;
        layout.inputs = kLightInputs;
        return layout;
    case Program::Composite:
    case Program::Filter:
        layout.primitive = gpu::Device::Primitive::Triangles;
        return layout;
    case Program::Recolor:
    case Program::Effect:
    case Program::Batch:
        layout.buffers[0] = {.stride = kCorner};
        layout.buffers[1] = {.stride = kPaired, .perInstance = true};
        layout.buffers[2] = {.stride = kPaired, .perInstance = true};
        layout.bufferCount = 3;
        layout.inputs = program == Program::Recolor ? std::span<const Input>(kRecolorInputs) : (program == Program::Effect ? std::span<const Input>(kEffectInputs) : std::span<const Input>(kBatchInputs));
        return layout;
    case Program::Shape:
    case Program::Sprite:
    case Program::Text:
        break;
    }
    layout.buffers[0] = {.stride = kCorner};
    layout.buffers[1] = {.stride = kInstance, .perInstance = true};
    layout.bufferCount = 2;
    layout.inputs = program == Program::Shape ? std::span<const Input>(kShapeInputs) : std::span<const Input>(kSpriteInputs);
    return layout;
}

RendererState::Targets RendererState::describeTargets(std::uint8_t blend, graphics::PassTarget target) const {
    using Mode = graphics::BlendMode::Type;
    const auto mode = static_cast<Mode>(blend);
    const gpu::Device::PixelFormat offscreen = device.getState().offscreenFormat;
    Targets targets;
    switch (target) {
    case graphics::PassTarget::Swapchain:
        // The swapchain of an opaque window keeps the alpha of its clear, so no blend can let the desktop through.
        targets.colors[0] = {.writeMask = gpu::Device::ColorMask::Rgb, .blend = blendState(mode)};
        return targets;
    case graphics::PassTarget::TransparentSwapchain:
        targets.colors[0] = {.blend = blendState(mode)};
        return targets;
    case graphics::PassTarget::Offscreen:
        targets.colors[0] = {.format = offscreen, .blend = blendState(mode)};
        return targets;
    case graphics::PassTarget::LightMap:
        targets.colors[0] = {.format = lightFormat, .writeMask = gpu::Device::ColorMask::Rgb, .blend = lightBlend(blend)};
        return targets;
    case graphics::PassTarget::LitScene:
        break;
    }

    // Every color of a lit pass takes the blend and the write mask of the draw, as OpenGL applies them to every color of a pass, and the shader library writes what keeps the emission, the surface and the info as they were where a draw must leave them.
    targets.count = 4;
    for (std::size_t index = 0; index < targets.count; ++index) {
        targets.colors[index] = {.format = offscreen, .blend = blendState(mode)};
    }
    return targets;
}

// Every input of the layout reads the vertex attribute of the program that has its name, wherever the compiler placed it.
gpu::Device::Pipeline RendererState::makePipeline(const graphics::ShaderResource& package, std::string_view programName, gpu::Device::Program made, Program program, std::uint8_t blend, graphics::PassTarget target, std::string_view label) {
    const Layout layout = describeLayout(program);
    const Targets targets = describeTargets(blend, target);
    const graphics::ShaderResource::ProgramInfo& info = package.getInfo(programName);
    std::array<gpu::Device::VertexAttribute, gpu::Device::kMaxAttributes> attributes{};
    std::size_t count = 0;
    for (const Input& input : layout.inputs) {
        const graphics::ShaderResource::Attribute* attribute = info.findAttribute(input.name);
        if (attribute == nullptr) {
            throw std::invalid_argument(std::format("The program \"{}/{}\" has no vertex input \"{}\". Compile it again with the shader library of this engine.", package.name, programName, input.name));
        }
        attributes[count++] = {.location = attribute->location, .buffer = input.buffer, .offset = input.offset, .format = input.format};
    }
    const bool swapchain = target == graphics::PassTarget::Swapchain || target == graphics::PassTarget::TransparentSwapchain;
    return getGpu().makePipeline({
        .program = made,
        .buffers = std::span(layout.buffers.data(), layout.bufferCount),
        .attributes = std::span(attributes.data(), count),
        .primitive = layout.primitive,
        .indexType = layout.indexType,
        .colors = std::span(targets.colors.data(), targets.count),
        .swapchain = swapchain,
        .label = label,
    });
}

void RendererState::loadPrograms() {
    packages.clear();
    for (const auto bytes : kPackages) {
        graphics::Shader package = graphics::Shader::parse(bytes());
        package.getResource()->graveyard = device.getState().graveyard;
        package.getResource()->prepare(getGpu(), jobs);
        packages.push_back(std::move(package));
    }
}

const graphics::ShaderResource::Compiled& RendererState::getProgram(Program program, bool lit) {
    const ProgramSource& source = kPrograms[static_cast<std::size_t>(program)];
    if (lit && source.lit.empty()) {
        throw std::logic_error("The program has no version for lit canvases.");
    }
    return packages[source.package].getResource()->getProgram(getGpu(), lit ? source.lit : source.program);
}

gpu::Device::Pipeline RendererState::getPipeline(Program program, std::uint8_t blend, graphics::PassTarget target) {
    const std::uint32_t key = pipelineKey(program, blend, target);
    if (const auto found = pipelines.find(key); found != pipelines.end()) {
        return found->second;
    }
    const bool lit = target == graphics::PassTarget::LitScene;
    const ProgramSource& source = kPrograms[static_cast<std::size_t>(program)];
    const graphics::ShaderResource::Compiled& made = getProgram(program, lit);
    const gpu::Device::Pipeline created = makePipeline(*packages[source.package].getResource(), lit ? source.lit : source.program, made.program, program, blend, target, "haylen-pipeline");
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
    case Program::Shape:
        return lit ? "shape_lit" : "shape";
    case Program::Batch:
        return lit ? "batch_lit" : "batch";
    case Program::ImageBlend:
    case Program::Backdrop:
    case Program::Composite:
    case Program::Light:
    case Program::Metaball:
    case Program::Recolor:
    case Program::Filter:
    case Program::Effect:
        break;
    }
    throw std::logic_error("Materials only replace the sprite, batch, text, mesh and shape programs.");
}

const graphics::ShaderResource::Compiled& RendererState::getMaterialProgram(MaterialResource& material, Program program, graphics::PassTarget target) {
    // The shader hands its GPU objects to the device graveyard once it changes or goes away.
    graphics::ShaderResource& shader = *material.shader.getResource();
    if (shader.graveyard.expired()) {
        shader.graveyard = device.getState().graveyard;
    }
    const graphics::ShaderResource::Compiled& made = shader.getProgram(getGpu(), materialProgramName(program, target));

    // The renderer fills the blocks of the shader library itself, so a shader compiled with another version of the library cannot draw.
    const bool lit = target == graphics::PassTarget::LitScene;
    if (made.blockSizes[kVertexBlock] != kVertexBlockSize || (lit && made.blockSizes[kLitBlock] != kLitBlockSize)) {
        throw std::invalid_argument(std::format("The shader \"{}\" was compiled with another version of the shader library. Compile it again with \"haylen.py shaders\".", shader.name));
    }
    return made;
}

gpu::Device::Pipeline RendererState::getMaterialPipeline(MaterialResource& material, Program program, std::uint8_t blend, graphics::PassTarget target) {
    graphics::ShaderResource& shader = *material.shader.getResource();
    const std::uint32_t key = pipelineKey(program, blend, target);
    if (const auto found = shader.pipelines.find(key); found != shader.pipelines.end()) {
        return found->second;
    }
    const char* programName = materialProgramName(program, target);
    const graphics::ShaderResource::Compiled& made = getMaterialProgram(material, program, target);
    const std::string label = std::format("{}/{}", shader.name, programName);
    const gpu::Device::Pipeline created = makePipeline(shader, programName, made.program, program, blend, target, label);
    shader.pipelines.emplace(key, created);
    return created;
}

void RendererState::ensureBuffer(gpu::Device::Buffer& buffer, std::size_t& capacity, std::size_t needed, std::size_t elementSize, bool isIndexBuffer) {
    if (needed <= capacity) {
        return;
    }
    if (buffer.isValid()) {
        device.getState().graveyard->bury(buffer);
    }
    const std::size_t grown = std::max<std::size_t>(needed + needed / 2, 1024);
    bufferBytes += (grown - capacity) * elementSize;
    buffersTracked.setBytes(bufferBytes);
    capacity = grown;
    buffer = getGpu().makeBuffer({.size = capacity * elementSize, .usage = gpu::Device::Usage::Dynamic, .index = isIndexBuffer, .label = isIndexBuffer ? "haylen-index-buffer" : "haylen-vertex-buffer"});
}

const std::array<std::span<const std::uint8_t> (*)(), 11> RendererState::kPackages{&core::EmbeddedFiles::getSpriteShader, &core::EmbeddedFiles::getTextShader, &core::EmbeddedFiles::getMeshShader, &core::EmbeddedFiles::getBlendShader, &core::EmbeddedFiles::getCompositeShader, &core::EmbeddedFiles::getLightShader, &core::EmbeddedFiles::getMetaballShader, &core::EmbeddedFiles::getRecolorShader, &core::EmbeddedFiles::getEffectShader, &core::EmbeddedFiles::getShapeShader, &core::EmbeddedFiles::getBackdropShader};

} // namespace haylen::graphics2d
