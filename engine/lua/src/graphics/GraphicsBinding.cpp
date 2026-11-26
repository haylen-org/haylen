#include "graphics/GraphicsBinding.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
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

int GraphicsBinding::newRenderTarget(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createRenderTarget(lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), lua::TypeConverter::readTextureOptions(L, 3)));
    return 1;
}

// Creates a texture from raw RGBA bytes or a fill color with `newTexture(width, height, {pixels = bytes or fill = color, dynamic, filter, wrap})`, where a dynamic texture changes its pixels with `update`, or from the bytes of an image file with `newTexture(bytes, {dynamic, filter, wrap})`, where bytes are a string or a data view.
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
    const haylen::graphics::Device::TextureData data{.width = width, .height = height, .format = format, .levels = levels};
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

// Replaces the pixels of a dynamic texture with `texture:update(pixels, {region})`, every RGBA byte of the texture in a string or a data view, which the upload reads in place, of which only the region, a rectangle in whole pixels, changed. Without a region every pixel changed.
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
    lua_getfield(L, 3, "region");
    const bool changedPart = !lua_isnil(L, -1);
    lua_pop(L, 1);
    if (!changedPart) {
        device.updateTexture(texture, pixels);
        return 0;
    }

    haylen::math::Rect region;
    lua::Table::readField(L, 3, "region", region);
    const auto whole = [](float value) { return std::trunc(value) == value; };
    if (!whole(region.x) || !whole(region.y) || !whole(region.width) || !whole(region.height)) {
        throw std::invalid_argument("The region of \"texture:update\" must hold whole pixels.");
    }

    device.updateTexture(texture, pixels, {static_cast<int>(region.x), static_cast<int>(region.y), static_cast<int>(region.width), static_cast<int>(region.height)});
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
    lua_createtable(L, 0, 11);
    setField(L, "maxTextureSize", device.maxImageSize);
    setField(L, "maxArrayLayers", device.maxArrayLayers);
    setField(L, "maxVolumeSize", device.maxVolumeSize);
    setField(L, "maxCubeSize", device.maxCubeSize);
    setField(L, "originTopLeft", device.originTopLeft);
    lua::Stack::push(L, device.depthRange);
    lua_setfield(L, -2, "depthRange");
    setField(L, "independentBlend", device.independentBlend);
    setField(L, "gpuTiming", device.gpuTiming);
    setField(L, "maxAnisotropy", device.maxAnisotropy);
    lua::Stack::push(L, device.maxLodBias);
    lua_setfield(L, -2, "maxLodBias");
    setField(L, "borderColors", device.borderColors);
    return 1;
}

// What the device does with the format, with the sample counts that its attachments take from one sample up, and none for a format that the device draws not into.
int GraphicsBinding::formatSupport(lua_State* L) {
    const auto format = lua::Stack::read<haylen::gpu::Device::PixelFormat>(L, 1);
    const haylen::gpu::Device::FormatSupport& support = lua::Runtime::getEngine(L).getGraphics().getGpu().getCapabilities().getFormat(format);
    lua_createtable(L, 0, 6);
    setField(L, "sample", support.sample);
    setField(L, "filter", support.filter);
    setField(L, "render", support.render);
    setField(L, "blend", support.blend);
    setField(L, "storage", support.storage);
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
    lua::ClassBuilder<haylen::graphics::Texture>(L)
        .function("update", &lua::Binding::native<&updateTexture>)
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
        .meta("__eq", &lua::Userdata::equal<haylen::graphics::Texture>)
        .install();
    // clang-format on
    lua::ClassBuilder<haylen::graphics::RenderTarget>(L).property("width", &targetWidth).property("height", &targetHeight).property("texture", &targetTexture).meta("__eq", &lua::Userdata::equal<haylen::graphics::RenderTarget>).install();
    lua::ClassBuilder<haylen::graphics::Shader>(L).property("name", &shaderName).property("uniforms", &shaderUniforms).property("textures", &shaderTextures).meta("__eq", &lua::Userdata::equal<haylen::graphics::Shader>).install();
    lua::ClassBuilder<haylen::graphics::VectorImage>(L).property("width", &vectorWidth).property("height", &vectorHeight).function("rasterize", &lua::Binding::native<&rasterizeVector>).meta("__eq", &lua::Userdata::equal<haylen::graphics::VectorImage>).install();
    FontBinding::install(L);
    lua::Binding::preload(L, "haylen.graphics", &open);
}

} // namespace haylen::lua::graphics
