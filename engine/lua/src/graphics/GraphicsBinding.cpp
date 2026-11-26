#include "graphics/GraphicsBinding.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <format>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/FontBinding.hpp"
#include "graphics/ImageBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Rect.hpp"
#include "varn/async/Promise.h"
#include "varn/dataview/DataViewModule.h"

namespace haylen::lua::graphics {

// Creates a buffer with `newBuffer(size or bytes, {usage = {...}, update, label})`, which starts with zeros or with the bytes, a string or a data view, and takes their size.
int GraphicsBinding::newBuffer(lua_State* L) {
    luaL_checktype(L, 2, LUA_TTABLE);
    lua::Table::checkFields(L, 2, {kBufferFields});
    haylen::graphics::Buffer::Options options;
    lua_getfield(L, 2, "usage");
    options.usage = readUsage(L, -1);
    lua_pop(L, 1);
    lua::Table::readField(L, 2, "update", options.update);
    std::string label;
    lua::Table::readField(L, 2, "label", label);
    options.label = label;

    haylen::graphics::Device& device = lua::Runtime::getEngine(L).getGraphics();
    if (lua_type(L, 1) == LUA_TNUMBER) {
        const lua_Integer size = luaL_checkinteger(L, 1);
        if (size <= 0) {
            throw std::invalid_argument("The size of \"graphics.newBuffer\" must be a positive number of bytes.");
        }

        lua::Stack::push(L, device.createBuffer(static_cast<std::size_t>(size), options));
        return 1;
    }

    const std::string_view bytes = lua::TypeConverter::readBytes(L, 1);
    lua::Stack::push(L, device.createBuffer(std::as_bytes(std::span(bytes.data(), bytes.size())), options));
    return 1;
}

haylen::graphics::Buffer::Usage GraphicsBinding::readUsage(lua_State* L, int index) {
    const int list = lua_absindex(L, index);
    if (!lua_istable(L, list)) {
        throw std::invalid_argument("The \"usage\" of \"graphics.newBuffer\" must be a list of usage names, such as {\"vertex\"}.");
    }

    haylen::graphics::Buffer::Usage usage;
    const auto count = static_cast<lua_Integer>(lua_rawlen(L, list));
    for (lua_Integer item = 1; item <= count; ++item) {
        lua_rawgeti(L, list, item);
        const std::string_view name = lua::Stack::read<std::string_view>(L, -1);
        lua_pop(L, 1);
        if (!haylen::graphics::Buffer::addUsage(usage, name)) {
            throw std::invalid_argument(std::format("The buffer usage \"{}\" does not exist. Use \"vertex\", \"index\", \"uniform\", \"storage\", \"indirect\", \"copySource\" or \"copyDestination\".", name));
        }
    }

    return usage;
}

int GraphicsBinding::bufferSize(lua_State* L) {
    lua::Stack::push(L, static_cast<lua_Integer>(lua::Userdata::check<haylen::graphics::Buffer>(L, 1).getSize()));
    return 1;
}

int GraphicsBinding::bufferUsage(lua_State* L) {
    const haylen::graphics::Buffer& buffer = lua::Userdata::check<haylen::graphics::Buffer>(L, 1);
    lua_newtable(L);
    lua_Integer item = 0;
    // clang-format off
    haylen::graphics::Buffer::visitUsages(buffer.getUsage(), [L, &item](std::string_view name) {
        lua_pushlstring(L, name.data(), name.size());
        lua_rawseti(L, -2, ++item);
    });
    // clang-format on
    return 1;
}

int GraphicsBinding::bufferUpdate(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Buffer>(L, 1).getUpdate());
    return 1;
}

int GraphicsBinding::bufferLabel(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Buffer>(L, 1).getLabel());
    return 1;
}

// Writes bytes into the buffer with `buffer:write(offset, bytes)`, from a string or a data view.
int GraphicsBinding::writeBuffer(lua_State* L) {
    const haylen::graphics::Buffer& buffer = lua::Userdata::check<haylen::graphics::Buffer>(L, 1);
    const lua_Integer offset = luaL_checkinteger(L, 2);
    if (offset < 0) {
        throw std::invalid_argument("The offset of \"buffer:write\" must not be negative.");
    }

    const std::string_view bytes = lua::TypeConverter::readBytes(L, 3);
    lua::Runtime::getEngine(L).getGraphics().writeBuffer(buffer, static_cast<std::size_t>(offset), std::as_bytes(std::span(bytes.data(), bytes.size())));
    return 0;
}

// Copies bytes of another buffer into this one with `buffer:copy(source, {sourceOffset, offset, size})`, the whole source at offset 0 by default.
int GraphicsBinding::copyBuffer(lua_State* L) {
    const haylen::graphics::Buffer& destination = lua::Userdata::check<haylen::graphics::Buffer>(L, 1);
    const haylen::graphics::Buffer& source = lua::Userdata::check<haylen::graphics::Buffer>(L, 2);
    lua_Integer sourceOffset = 0;
    lua_Integer offset = 0;
    auto size = static_cast<lua_Integer>(source.getSize());
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kCopyFields});
        lua::Table::readField(L, 3, "sourceOffset", sourceOffset);
        lua::Table::readField(L, 3, "offset", offset);
        lua::Table::readField(L, 3, "size", size);
    }

    if (sourceOffset < 0 || offset < 0 || size <= 0) {
        throw std::invalid_argument("The offsets of \"buffer:copy\" must not be negative, and its size must be positive.");
    }

    lua::Runtime::getEngine(L).getGraphics().copyBuffer(source, static_cast<std::size_t>(sourceOffset), destination, static_cast<std::size_t>(offset), static_cast<std::size_t>(size));
    return 0;
}

// Reads bytes of the buffer with `buffer:readAsync([offset, size])`, the whole buffer by default, and returns a promise of a string of the bytes as the frame that asks for them leaves the buffer.
int GraphicsBinding::readBufferAsync(lua_State* L) {
    const haylen::graphics::Buffer& buffer = lua::Userdata::check<haylen::graphics::Buffer>(L, 1);
    const lua_Integer offset = luaL_optinteger(L, 2, 0);
    const lua_Integer size = luaL_optinteger(L, 3, static_cast<lua_Integer>(buffer.getSize()) - offset);
    if (offset < 0 || size <= 0) {
        throw std::invalid_argument("The offset of \"buffer:readAsync\" must not be negative, and its size must be positive.");
    }

    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getGraphics().readBuffer(buffer, static_cast<std::size_t>(offset), static_cast<std::size_t>(size), [promise](std::optional<std::vector<std::uint8_t>> bytes, const std::string& error) {
        if (!bytes) {
            promise->reject(error);
            return;
        }

        auto shared = std::make_shared<std::vector<std::uint8_t>>(std::move(*bytes));
        promise->resolveCustom([shared](lua_State* state) { lua_pushlstring(state, reinterpret_cast<const char*>(shared->data()), shared->size()); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int GraphicsBinding::newRenderTarget(lua_State* L) {
    const int width = lua::Stack::read<int>(L, 1);
    const int height = lua::Stack::read<int>(L, 2);
    const haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 3, {kTargetFields});
    const haylen::graphics::RenderTarget::Options attachments = readTargetOptions(L, 3);
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createRenderTarget(width, height, options, attachments));
    return 1;
}

// Creates a texture from raw RGBA bytes or a fill color with `newTexture(width, height, {pixels = bytes or fill = color, dynamic, filter, wrap})`, where a dynamic texture changes its pixels with `update`, or from the bytes of an image file with `newTexture(bytes, {dynamic, filter, wrap})`, where bytes are a string or a data view.
haylen::graphics::RenderTarget::Options GraphicsBinding::readTargetOptions(lua_State* L, int index) {
    haylen::graphics::RenderTarget::Options attachments;
    if (lua_isnoneornil(L, index)) {
        return attachments;
    }

    lua::Table::readField(L, index, "colors", attachments.colors);
    lua::Table::readField(L, index, "depthFormat", attachments.depthFormat);
    lua::Table::readField(L, index, "samples", attachments.samples);
    lua::Table::readField(L, index, "depthTexture", attachments.depthTexture);
    lua::Table::readField(L, index, "depthResolve", attachments.depthResolve);
    lua::Table::readField(L, index, "memoryless", attachments.memoryless);
    return attachments;
}

int GraphicsBinding::newTexture(lua_State* L) {
    if (lua_type(L, 1) == LUA_TSTRING || luaL_testudata(L, 1, varn::dataview::DataViewModule::kMetatable) != nullptr) {
        pushImageTexture(L);
        return 1;
    }

    if (const haylen::graphics::Image* image = lua::Userdata::test<haylen::graphics::Image>(L, 1)) {
        const haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 2);
        lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createTexture(*image, options));
        return 1;
    }

    const int width = lua::Stack::read<int>(L, 1);
    const int height = lua::Stack::read<int>(L, 2);
    if (lua_istable(L, 3) && (hasField(L, 3, "format") || hasField(L, 3, "levels"))) {
        pushDataTexture(L, width, height);
        return 1;
    }

    haylen::math::Color fill = haylen::math::Color::white();
    std::optional<std::string_view> pixels;
    bool dynamic = false;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::readField(L, 3, "fill", fill);
        lua::Table::readField(L, 3, "dynamic", dynamic);
        lua_getfield(L, 3, "pixels");
        if (!lua_isnil(L, -1)) {
            pixels = lua::TypeConverter::readBytes(L, -1);
        }
    }

    const haylen::graphics::Device::TextureData shape = readShape(L, width, height);
    if (shape.type != haylen::gpu::Device::TextureType::Flat || shape.layers != 1 || shape.depth != 1) {
        pushLayeredTexture(L, shape, pixels, fill, dynamic);
        return 1;
    }

    haylen::graphics::Device& device = lua::Runtime::getEngine(L).getGraphics();
    const haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 3, {kTextureContentFields});
    if (!pixels) {
        lua::Stack::push(L, dynamic ? device.createDynamicTexture(width, height, fill, options) : device.createTexture(width, height, fill, options));
        return 1;
    }

    const haylen::graphics::Image image(width, height, std::vector<std::uint8_t>(pixels->begin(), pixels->end()));
    lua::Stack::push(L, dynamic ? device.createDynamicTexture(image, options) : device.createTexture(image, options));
    return 1;
}

haylen::graphics::Device::TextureData GraphicsBinding::readShape(lua_State* L, int width, int height) {
    haylen::graphics::Device::TextureData shape{.width = width, .height = height};
    if (lua_isnoneornil(L, 3)) {
        return shape;
    }

    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Table::readField(L, 3, "kind", shape.type);
    const bool cube = shape.type == haylen::gpu::Device::TextureType::Cube || shape.type == haylen::gpu::Device::TextureType::CubeArray;
    shape.layers = cube ? 6 : 1;
    lua::Table::readField(L, 3, "layers", shape.layers);
    lua::Table::readField(L, 3, "depth", shape.depth);
    return shape;
}

// The pixels hold every layer of the first level one after another, four bytes a pixel, and a texture that changes in place takes new pixels for each layer with `update`.
void GraphicsBinding::pushLayeredTexture(lua_State* L, const haylen::graphics::Device::TextureData& shape, std::optional<std::string_view> pixels, haylen::math::Color fill, bool dynamic) {
    haylen::graphics::Device& device = lua::Runtime::getEngine(L).getGraphics();
    haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 3, {kTextureContentFields});
    if (!pixels) {
        lua::Stack::push(L, dynamic ? device.createDynamicTexture(shape, fill, options) : device.createTexture(shape, fill, options));
        return;
    }

    const std::array<std::span<const std::byte>, 1> levels{std::as_bytes(std::span(pixels->data(), pixels->size()))};
    haylen::graphics::Device::TextureData data = shape;
    data.levels = levels;
    if (dynamic) {
        lua::Stack::push(L, device.createDynamicTexture(data, options));
        return;
    }

    options.mipmaps = options.hasMipmaps();
    lua::Stack::push(L, device.createTexture(data, options));
}

// Makes a view of a range of the mip levels and layers of a texture with `texture:view({kind, mipLevel, mipLevels, layer, layers})`, which takes the kind of the texture, every level from the first one and every layer from the first one by default.
int GraphicsBinding::viewTexture(lua_State* L) {
    const haylen::graphics::Texture& texture = lua::Userdata::check<haylen::graphics::Texture>(L, 1);
    haylen::graphics::Device::TextureView view{.type = texture.getType()};
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kViewFields});
        lua::Table::readField(L, 2, "kind", view.type);
        lua::Table::readField(L, 2, "mipLevel", view.mipLevel);
        lua::Table::readField(L, 2, "layer", view.layer);
    }

    const bool whole = view.type == haylen::gpu::Device::TextureType::Flat || view.type == haylen::gpu::Device::TextureType::Volume;
    view.mipLevels = texture.getMipLevels() - view.mipLevel;
    view.layers = whole ? 1 : (view.type == haylen::gpu::Device::TextureType::Cube ? 6 : texture.getLayers() - view.layer);
    if (!lua_isnoneornil(L, 2)) {
        lua::Table::readField(L, 2, "mipLevels", view.mipLevels);
        lua::Table::readField(L, 2, "layers", view.layers);
    }

    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createView(texture, view));
    return 1;
}

bool GraphicsBinding::hasField(lua_State* L, int index, const char* name) {
    lua_getfield(L, index, name);
    const bool present = !lua_isnil(L, -1);
    lua_pop(L, 1);
    return present;
}

// A texture made from its levels takes the bytes of each level from the list `levels`, which the table of options keeps alive while the device copies them.
void GraphicsBinding::pushDataTexture(lua_State* L, int width, int height) {
    haylen::gpu::Device::PixelFormat format = haylen::gpu::Device::PixelFormat::Rgba8;
    lua::Table::readField(L, 3, "format", format);
    lua_getfield(L, 3, "levels");
    if (!lua_istable(L, -1)) {
        throw std::invalid_argument("A texture made in a format takes the bytes of each of its levels in the list \"levels\", the largest level first.");
    }

    const auto count = static_cast<int>(lua_rawlen(L, -1));
    std::vector<std::span<const std::byte>> levels;
    levels.reserve(static_cast<std::size_t>(count));
    for (int index = 1; index <= count; ++index) {
        lua_rawgeti(L, -1, index);
        const std::string_view bytes = lua::TypeConverter::readBytes(L, -1);
        levels.push_back(std::as_bytes(std::span(bytes.data(), bytes.size())));
        lua_pop(L, 1);
    }

    lua_pop(L, 1);
    const haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 3, {kTextureDataFields});
    haylen::graphics::Device::TextureData data = readShape(L, width, height);
    data.format = format;
    data.levels = levels;
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createTexture(data, options));
}

void GraphicsBinding::pushImageTexture(lua_State* L) {
    const std::string_view bytes = lua::TypeConverter::readBytes(L, 1);
    bool dynamic = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::readField(L, 2, "dynamic", dynamic);
    }

    const haylen::graphics::Texture::Options options = lua::TypeConverter::readTextureOptions(L, 2, {kImageTextureFields});
    const haylen::graphics::Image image = haylen::graphics::Image::decodeFor({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}, options);
    haylen::graphics::Device& device = lua::Runtime::getEngine(L).getGraphics();
    lua::Stack::push(L, dynamic ? device.createDynamicTexture(image, options) : device.createTexture(image, options));
}

// Replaces the pixels of a layer of a dynamic texture with `texture:update(pixels, {region, layer})`, every RGBA byte of the layer in a string or a data view, which the upload reads in place, of which only the region, a rectangle in whole pixels, changed. Without a region every pixel changed, and without a layer the first one.
int GraphicsBinding::updateTexture(lua_State* L) {
    const haylen::graphics::Texture& texture = lua::Userdata::check<haylen::graphics::Texture>(L, 1);
    const std::string_view bytes = lua::TypeConverter::readBytes(L, 2);
    const std::span pixels(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size());
    haylen::graphics::Device& device = lua::Runtime::getEngine(L).getGraphics();
    if (lua_isnoneornil(L, 3)) {
        device.updateTexture(texture, pixels);
        return 0;
    }

    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Table::checkFields(L, 3, {kUpdateFields});
    int layer = 0;
    lua::Table::readField(L, 3, "layer", layer);
    lua_getfield(L, 3, "region");
    const bool changedPart = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!changedPart) {
        device.updateTexture(texture, layer, pixels, {0, 0, texture.getWidth(), texture.getHeight()});
        return 0;
    }

    haylen::math::Rect region;
    lua::Table::readField(L, 3, "region", region);
    const auto whole = [](float value) { return std::trunc(value) == value; };
    if (!whole(region.x) || !whole(region.y) || !whole(region.width) || !whole(region.height)) {
        throw std::invalid_argument("The region of \"texture:update\" must hold whole pixels.");
    }

    device.updateTexture(texture, layer, pixels, {static_cast<int>(region.x), static_cast<int>(region.y), static_cast<int>(region.width), static_cast<int>(region.height)});
    return 0;
}

int GraphicsBinding::maxTextureSize(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getMaxTextureSize());
    return 1;
}

int GraphicsBinding::trim(lua_State* L) {
    lua::Runtime::getEngine(L).getGraphics().trim();
    return 0;
}

void GraphicsBinding::setField(lua_State* L, const char* name, int value) {
    lua::Stack::push(L, value);
    lua_setfield(L, -2, name);
}

void GraphicsBinding::setField(lua_State* L, const char* name, bool value) {
    lua::Stack::push(L, value);
    lua_setfield(L, -2, name);
}

int GraphicsBinding::capabilities(lua_State* L) {
    const haylen::gpu::Device::Capabilities& device = lua::Runtime::getEngine(L).getGraphics().getGpu().getCapabilities();
    lua_createtable(L, 0, 20);
    setField(L, "maxTextureSize", device.maxImageSize);
    setField(L, "maxArrayLayers", device.maxArrayLayers);
    setField(L, "maxVolumeSize", device.maxVolumeSize);
    setField(L, "maxCubeSize", device.maxCubeSize);
    setField(L, "cubeArrays", device.cubeArrays);
    setField(L, "imageViews", device.imageViews);
    setField(L, "depthLayers", device.depthLayers);
    setField(L, "originTopLeft", device.originTopLeft);
    lua::Stack::push(L, device.depthRange);
    lua_setfield(L, -2, "depthRange");
    setField(L, "independentBlend", device.independentBlend);
    lua_createtable(L, static_cast<int>(haylen::gpu::Device::kDepthResolveNames.size()), 0);
    int resolves = 0;
    for (const auto& [name, mode] : haylen::gpu::Device::kDepthResolveNames) {
        if (device.hasDepthResolve(mode)) {
            lua::Stack::push(L, name);
            lua_rawseti(L, -2, ++resolves);
        }
    }

    lua_setfield(L, -2, "depthResolves");
    setField(L, "memorylessAttachments", device.memorylessAttachments);
    setField(L, "gpuTiming", device.gpuTiming);
    setField(L, "maxAnisotropy", device.maxAnisotropy);
    lua::Stack::push(L, device.maxLodBias);
    lua_setfield(L, -2, "maxLodBias");
    setField(L, "borderColors", device.borderColors);
    setField(L, "storageBuffers", device.storageBuffers);
    setField(L, "indirectBuffers", device.indirectBuffers);
    lua::Stack::push(L, static_cast<lua_Integer>(device.maxUniformBlockSize));
    lua_setfield(L, -2, "maxUniformBlockSize");
    lua::Stack::push(L, static_cast<lua_Integer>(haylen::gpu::Device::kUniformBufferAlignment));
    lua_setfield(L, -2, "uniformBufferAlignment");
    return 1;
}

// What the device does with the format, with the sample counts that its attachments take from one sample up, and none for a format that the device draws not into.
int GraphicsBinding::formatSupport(lua_State* L) {
    const auto format = lua::Stack::read<haylen::gpu::Device::PixelFormat>(L, 1);
    const haylen::gpu::Device::FormatSupport& support = lua::Runtime::getEngine(L).getGraphics().getGpu().getCapabilities().getFormat(format);
    lua_createtable(L, 0, 7);
    setField(L, "sample", support.sample);
    setField(L, "filter", support.filter);
    setField(L, "render", support.render);
    setField(L, "blend", support.blend);
    setField(L, "storage", support.storage);
    setField(L, "mipmaps", support.mipmaps);
    lua_createtable(L, 4, 0);
    int index = 0;
    for (unsigned count = 1; count <= 128; count *= 2) {
        if ((support.sampleCounts & count) != 0) {
            lua::Stack::push(L, static_cast<int>(count));
            lua_rawseti(L, -2, ++index);
        }
    }

    lua_setfield(L, -2, "sampleCounts");
    return 1;
}

int GraphicsBinding::depthRange(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getGpu().getCapabilities().depthRange);
    return 1;
}

int GraphicsBinding::whiteTexture(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getWhiteTexture());
    return 1;
}

int GraphicsBinding::apiName(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getApiName());
    return 1;
}

int GraphicsBinding::textureWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getWidth());
    return 1;
}

int GraphicsBinding::textureHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getHeight());
    return 1;
}

int GraphicsBinding::textureFilter(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().filter);
    return 1;
}

int GraphicsBinding::textureFormat(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getFormat());
    return 1;
}

int GraphicsBinding::textureKind(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getType());
    return 1;
}

int GraphicsBinding::textureLayers(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getLayers());
    return 1;
}

int GraphicsBinding::textureDepth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getDepth());
    return 1;
}

int GraphicsBinding::textureMipLevels(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getMipLevels());
    return 1;
}

int GraphicsBinding::textureMipmaps(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().hasMipmaps());
    return 1;
}

int GraphicsBinding::textureWrapU(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().wrapU);
    return 1;
}

int GraphicsBinding::textureWrapV(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().wrapV);
    return 1;
}

int GraphicsBinding::textureWrapW(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().wrapW);
    return 1;
}

int GraphicsBinding::textureAnisotropy(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().anisotropy.value_or(1));
    return 1;
}

int GraphicsBinding::textureLodBias(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().lodBias);
    return 1;
}

int GraphicsBinding::textureMinLod(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().minLod);
    return 1;
}

int GraphicsBinding::textureMaxLod(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().maxLod);
    return 1;
}

int GraphicsBinding::textureBorderColor(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Texture>(L, 1).getOptions().borderColor);
    return 1;
}

int GraphicsBinding::targetTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getTexture());
    return 1;
}

int GraphicsBinding::targetWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getWidth());
    return 1;
}

int GraphicsBinding::targetHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getHeight());
    return 1;
}

int GraphicsBinding::targetTextures(lua_State* L) {
    const haylen::graphics::RenderTarget& target = lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1);
    lua_createtable(L, static_cast<int>(target.getColorCount()), 0);
    for (std::size_t index = 0; index < target.getColorCount(); ++index) {
        lua::Stack::push(L, target.getTexture(index));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int GraphicsBinding::targetDepthTexture(lua_State* L) {
    const haylen::graphics::Texture& depth = lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getDepthTexture();
    if (!depth.isValid()) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, depth);
    return 1;
}

int GraphicsBinding::targetColors(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getOptions().colors);
    return 1;
}

int GraphicsBinding::targetDepthFormat(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getOptions().depthFormat);
    return 1;
}

int GraphicsBinding::targetSamples(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getOptions().samples);
    return 1;
}

int GraphicsBinding::targetMemoryless(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::RenderTarget>(L, 1).getOptions().memoryless);
    return 1;
}

int GraphicsBinding::shaderName(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::Shader>(L, 1).getName());
    return 1;
}

// Lists the uniforms that materials of the shader fill, each as `{name = 'time', type = 'float', count = 1}`.
int GraphicsBinding::shaderUniforms(lua_State* L) {
    const std::vector<haylen::graphics::Shader::Uniform>& uniforms = lua::Userdata::check<haylen::graphics::Shader>(L, 1).getUniforms();
    lua_createtable(L, static_cast<int>(uniforms.size()), 0);
    for (std::size_t index = 0; index < uniforms.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, uniforms[index].name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, haylen::graphics::Shader::uniformTypeName(uniforms[index].type));
        lua_setfield(L, -2, "type");
        lua::Stack::push(L, uniforms[index].count);
        lua_setfield(L, -2, "count");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int GraphicsBinding::shaderTextures(lua_State* L) {
    const std::vector<haylen::graphics::Shader::TextureSlot>& textures = lua::Userdata::check<haylen::graphics::Shader>(L, 1).getTextures();
    lua_createtable(L, static_cast<int>(textures.size()), 0);
    for (std::size_t index = 0; index < textures.size(); ++index) {
        lua::Stack::push(L, textures[index].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Reads the text of an SVG document into a vector image with `newVectorImage(text)`.
int GraphicsBinding::newVectorImage(lua_State* L) {
    const std::string_view text = lua::Stack::read<std::string_view>(L, 1);
    lua::Stack::push(L, haylen::graphics::VectorImage::parse(std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size())));
    return 1;
}

int GraphicsBinding::vectorWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::VectorImage>(L, 1).getSize().x);
    return 1;
}

int GraphicsBinding::vectorHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics::VectorImage>(L, 1).getSize().y);
    return 1;
}

// Rasterizes the image on the task pool with `image:rasterize(scale[, {filter, wrap}])`, and returns a promise of the texture of the raster, which samples with linear filtering unless the options say otherwise.
int GraphicsBinding::rasterizeVector(lua_State* L) {
    const haylen::graphics::VectorImage image = lua::Userdata::check<haylen::graphics::VectorImage>(L, 1);
    const auto scale = lua::Stack::read<float>(L, 2);
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    const haylen::math::Vec2 size = image.getSize() * scale;
    if (!(scale > 0.0F && std::isfinite(scale)) || std::max(size.x, size.y) > static_cast<float>(engine.getGraphics().getMaxTextureSize())) {
        throw std::invalid_argument("A vector image rasterizes at a positive scale whose raster fits the maximum texture size of the device.");
    }

    haylen::graphics::Texture::Options options{.filter = haylen::graphics::Texture::Filter::Linear};
    if (!lua_isnoneornil(L, 3)) {
        options = lua::TypeConverter::readTextureOptions(L, 3);
        lua_getfield(L, 3, "filter");
        options.filter = lua_isnil(L, -1) ? haylen::graphics::Texture::Filter::Linear : options.filter;
        lua_pop(L, 1);
    }

    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // A texture that finds no room on the device rejects the promise like a raster that failed.
    // clang-format off
    engine.getJobs().run([image, scale] { return image.rasterize(scale); }, [promise, device = &engine.getGraphics(), options](haylen::core::JobSystem::Result<haylen::graphics::Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        haylen::graphics::Texture texture;
        try {
            texture = device->createTexture(*result.value, options);
        } catch (const std::exception& error) {
            promise->reject(error.what());
            return;
        }

        promise->resolveCustom([texture](lua_State* state) { lua::Stack::push(state, texture); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int GraphicsBinding::open(lua_State* L) {
    // clang-format off
    const luaL_Reg functions[] = {
        {"newBuffer", &lua::Binding::native<&newBuffer>},
        {"newTexture", &lua::Binding::native<&newTexture>},
        {"newRenderTarget", &lua::Binding::native<&newRenderTarget>},
        {"newVectorImage", &lua::Binding::native<&newVectorImage>},
        {"newImage", &lua::Binding::native<&ImageBinding::create>},
        {"decodeImage", &lua::Binding::native<&ImageBinding::decode>},
        {"maxImageSize", &ImageBinding::maxSize},
        {"decodeImageAsync", &lua::Binding::native<&ImageBinding::decodeAsync>},
        {"decodePlaceholder", &lua::Binding::native<&ImageBinding::decodePlaceholder>},
        {"readTarget", &lua::Binding::native<&ImageBinding::readTarget>},
        {"captureScreen", &lua::Binding::native<&ImageBinding::captureScreen>},
        {"whiteTexture", &whiteTexture},
        {"apiName", &apiName},
        {"maxTextureSize", &maxTextureSize},
        {"trim", &lua::Binding::native<&trim>},
        {"capabilities", &capabilities},
        {"formatSupport", &lua::Binding::native<&formatSupport>},
        {"depthRange", &depthRange},
        {"newFontFamily", &lua::Binding::native<&FontBinding::newFontFamily>},
        {"systemFonts", &FontBinding::systemFonts},
        {"newBitmapFont", &lua::Binding::native<&FontBinding::newBitmapFont>},
        {"newGridFont", &lua::Binding::native<&FontBinding::newGridFont>},
        {nullptr, nullptr},
    };
    // clang-format on
    lua::Binding::newModule(L, functions);
    return 1;
}

void GraphicsBinding::install(lua_State* L) {
    ImageBinding::install(L);
    // clang-format off
    lua::ClassBuilder<haylen::graphics::Buffer>(L)
        .function("write", &lua::Binding::native<&writeBuffer>)
        .function("copy", &lua::Binding::native<&copyBuffer>)
        .function("readAsync", &lua::Binding::native<&readBufferAsync>)
        .property("size", &bufferSize)
        .property("usage", &bufferUsage)
        .property("update", &bufferUpdate)
        .property("label", &bufferLabel)
        .meta("__eq", &lua::Userdata::equal<haylen::graphics::Buffer>)
        .install();
    lua::ClassBuilder<haylen::graphics::Texture>(L)
        .function("update", &lua::Binding::native<&updateTexture>)
        .function("view", &lua::Binding::native<&viewTexture>)
        .property("width", &textureWidth)
        .property("height", &textureHeight)
        .property("filter", &textureFilter)
        .property("wrapU", &textureWrapU)
        .property("wrapV", &textureWrapV)
        .property("wrapW", &textureWrapW)
        .property("anisotropy", &textureAnisotropy)
        .property("lodBias", &textureLodBias)
        .property("minLod", &textureMinLod)
        .property("maxLod", &textureMaxLod)
        .property("borderColor", &textureBorderColor)
        .property("mipmaps", &textureMipmaps)
        .property("format", &textureFormat)
        .property("kind", &textureKind)
        .property("layers", &textureLayers)
        .property("depth", &textureDepth)
        .property("mipLevels", &textureMipLevels)
        .meta("__eq", &lua::Userdata::equal<haylen::graphics::Texture>)
        .install();
    // clang-format on
    // clang-format off
    lua::ClassBuilder<haylen::graphics::RenderTarget>(L)
        .property("width", &targetWidth)
        .property("height", &targetHeight)
        .property("texture", &targetTexture)
        .property("textures", &targetTextures)
        .property("depthTexture", &targetDepthTexture)
        .property("colors", &targetColors)
        .property("depthFormat", &targetDepthFormat)
        .property("samples", &targetSamples)
        .property("memoryless", &targetMemoryless)
        .meta("__eq", &lua::Userdata::equal<haylen::graphics::RenderTarget>)
        .install();
    // clang-format on
    lua::ClassBuilder<haylen::graphics::Shader>(L).property("name", &shaderName).property("uniforms", &shaderUniforms).property("textures", &shaderTextures).meta("__eq", &lua::Userdata::equal<haylen::graphics::Shader>).install();
    lua::ClassBuilder<haylen::graphics::VectorImage>(L).property("width", &vectorWidth).property("height", &vectorHeight).function("rasterize", &lua::Binding::native<&rasterizeVector>).meta("__eq", &lua::Userdata::equal<haylen::graphics::VectorImage>).install();
    FontBinding::install(L);
    lua::Binding::preload(L, "haylen.graphics", &open);
}

} // namespace haylen::lua::graphics
