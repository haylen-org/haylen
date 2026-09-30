#include "graphics/GraphicsLua.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "graphics/FontLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics {

int GraphicsLua::newRenderTarget(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().createRenderTarget(lua::Stack::read<int>(L, 1), lua::Stack::read<int>(L, 2), lua::TypeConverter::readTextureOptions(L, 3)));
    return 1;
}

// Creates a texture from raw RGBA bytes or a fill color with `newTexture(width, height, {pixels = string or fill = color, dynamic, filter, wrap})`, where a dynamic texture changes its pixels with `update`, or from the bytes of an image file with `newTexture(bytes, {dynamic, filter, wrap})`.
int GraphicsLua::newTexture(lua_State* L) {
    if (lua_type(L, 1) == LUA_TSTRING) {
        pushImageTexture(L);
        return 1;
    }
    const int width = lua::Stack::read<int>(L, 1);
    const int height = lua::Stack::read<int>(L, 2);
    math::Color fill = math::Color::white();
    std::string pixels;
    bool dynamic = false;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::readField(L, 3, "fill", fill);
        lua::Table::readField(L, 3, "pixels", pixels);
        lua::Table::readField(L, 3, "dynamic", dynamic);
    }

    Device& device = lua::Runtime::getEngine(L).getGraphics();
    const Texture::Options options = lua::TypeConverter::readTextureOptions(L, 3, {kTextureContentFields});
    if (pixels.empty()) {
        lua::Stack::push(L, dynamic ? device.createDynamicTexture(width, height, fill, options) : device.createTexture(width, height, fill, options));
        return 1;
    }
    const Image image(width, height, std::vector<std::uint8_t>(pixels.begin(), pixels.end()));
    lua::Stack::push(L, dynamic ? device.createDynamicTexture(image, options) : device.createTexture(image, options));
    return 1;
}

void GraphicsLua::pushImageTexture(lua_State* L) {
    const std::string_view bytes = lua::Stack::read<std::string_view>(L, 1);
    bool dynamic = false;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::readField(L, 2, "dynamic", dynamic);
    }
    const Texture::Options options = lua::TypeConverter::readTextureOptions(L, 2, {kImageTextureFields});
    const Image image = Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    Device& device = lua::Runtime::getEngine(L).getGraphics();
    lua::Stack::push(L, dynamic ? device.createDynamicTexture(image, options) : device.createTexture(image, options));
}

// Replaces every pixel of a dynamic texture with `texture:update(pixels)`, a string of RGBA bytes of the same size.
int GraphicsLua::updateTexture(lua_State* L) {
    const Texture& texture = lua::Userdata::check<Texture>(L, 1);
    const std::string_view pixels = lua::Stack::read<std::string_view>(L, 2);
    lua::Runtime::getEngine(L).getGraphics().updateTexture(texture, {reinterpret_cast<const std::uint8_t*>(pixels.data()), pixels.size()});
    return 0;
}

int GraphicsLua::maxTextureSize(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getMaxTextureSize());
    return 1;
}

int GraphicsLua::whiteTexture(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getWhiteTexture());
    return 1;
}

int GraphicsLua::backendName(lua_State* L) {
    lua::Stack::push(L, lua::Runtime::getEngine(L).getGraphics().getBackendName());
    return 1;
}

int GraphicsLua::textureWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Texture>(L, 1).getWidth());
    return 1;
}

int GraphicsLua::textureHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Texture>(L, 1).getHeight());
    return 1;
}

int GraphicsLua::textureFilter(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Texture>(L, 1).getOptions().filter);
    return 1;
}

int GraphicsLua::textureWrap(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Texture>(L, 1).getOptions().wrap);
    return 1;
}

int GraphicsLua::targetTexture(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<RenderTarget>(L, 1).getTexture());
    return 1;
}

int GraphicsLua::targetWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<RenderTarget>(L, 1).getWidth());
    return 1;
}

int GraphicsLua::targetHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<RenderTarget>(L, 1).getHeight());
    return 1;
}

int GraphicsLua::shaderName(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Shader>(L, 1).getName());
    return 1;
}

// Lists the uniforms that materials of the shader fill, each as `{name = 'time', type = 'float', count = 1}`.
int GraphicsLua::shaderUniforms(lua_State* L) {
    const std::vector<Shader::Uniform>& uniforms = lua::Userdata::check<Shader>(L, 1).getUniforms();
    lua_createtable(L, static_cast<int>(uniforms.size()), 0);
    for (std::size_t index = 0; index < uniforms.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, uniforms[index].name);
        lua_setfield(L, -2, "name");
        lua::Stack::push(L, Shader::uniformTypeName(uniforms[index].type));
        lua_setfield(L, -2, "type");
        lua::Stack::push(L, uniforms[index].count);
        lua_setfield(L, -2, "count");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int GraphicsLua::shaderTextures(lua_State* L) {
    const std::vector<Shader::TextureSlot>& textures = lua::Userdata::check<Shader>(L, 1).getTextures();
    lua_createtable(L, static_cast<int>(textures.size()), 0);
    for (std::size_t index = 0; index < textures.size(); ++index) {
        lua::Stack::push(L, textures[index].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int GraphicsLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newTexture", &lua::Binding::native<&newTexture>}, {"newRenderTarget", &lua::Binding::native<&newRenderTarget>}, {"whiteTexture", &whiteTexture}, {"backendName", &backendName}, {"maxTextureSize", &maxTextureSize}, {"newFontFamily", &lua::Binding::native<&FontLua::newFontFamily>}, {"newBitmapFont", &lua::Binding::native<&FontLua::newBitmapFont>}, {"newGridFont", &lua::Binding::native<&FontLua::newGridFont>}, {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void GraphicsLua::install(lua_State* L) {
    lua::ClassBuilder<Texture>(L).function("update", &lua::Binding::native<&updateTexture>).property("width", &textureWidth).property("height", &textureHeight).property("filter", &textureFilter).property("wrap", &textureWrap).meta("__eq", &lua::Userdata::equal<Texture>).install();
    lua::ClassBuilder<RenderTarget>(L).property("width", &targetWidth).property("height", &targetHeight).property("texture", &targetTexture).meta("__eq", &lua::Userdata::equal<RenderTarget>).install();
    lua::ClassBuilder<Shader>(L).property("name", &shaderName).property("uniforms", &shaderUniforms).property("textures", &shaderTextures).meta("__eq", &lua::Userdata::equal<Shader>).install();
    FontLua::install(L);
    lua::Binding::preload(L, "haylen.graphics", &open);
}

} // namespace haylen::graphics
