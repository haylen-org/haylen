#include "2d/graphics/SpriteBatchLua.hpp"

#include <algorithm>
#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/SpriteBatch.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct EnumNames<graphics2d::SpriteLayout::Field> {
    static std::optional<graphics2d::SpriteLayout::Field> fromName(std::string_view name) {
        return graphics2d::SpriteLayout::fieldFromName(name);
    }
    static std::string_view name(graphics2d::SpriteLayout::Field value) {
        return graphics2d::SpriteLayout::fieldName(value);
    }
};

} // namespace haylen::lua

namespace haylen::graphics2d {

std::vector<graphics::Texture> SpriteBatchLua::readTextures(lua_State* L, int index) {
    if (lua_type(L, index) == LUA_TTABLE) {
        return lua::Stack::read<std::vector<graphics::Texture>>(L, index);
    }
    return {lua::Stack::read<graphics::Texture>(L, index)};
}

// The data holds up to four numbers, and the numbers it leaves out keep those of the base.
std::optional<SpriteData> SpriteBatchLua::readSpriteData(lua_State* L, int index, std::size_t textureCount, SpriteData base) {
    const int table = lua_absindex(L, index);
    const int texture = lua_getfield(L, table, "texture");
    const int values = lua_getfield(L, table, "data");
    if (texture == LUA_TNIL && values == LUA_TNIL) {
        lua_pop(L, 2);
        return std::nullopt;
    }
    if (texture != LUA_TNIL) {
        const lua_Integer number = luaL_checkinteger(L, -2);
        if (number < 1 || static_cast<std::size_t>(number) > textureCount) {
            throw std::invalid_argument(std::format("A sprite draws from the texture {} of its batch, which has {}.", number, textureCount == 1 ? std::string("1 texture") : std::format("{} textures", textureCount)));
        }
        base.texture = static_cast<std::uint8_t>(number - 1);
    }
    if (values != LUA_TNIL) {
        const std::vector<float> numbers = lua::Stack::read<std::vector<float>>(L, -1);
        if (numbers.size() > base.values.size()) {
            throw std::invalid_argument(std::format("The data of a sprite holds up to 4 numbers, and this one has {}.", numbers.size()));
        }
        std::ranges::copy(numbers, base.values.begin());
    }
    lua_pop(L, 2);
    return base;
}

SpriteLayout SpriteBatchLua::readLayout(lua_State* L, int index, const SpriteInstance& sprite, const SpriteData& data) {
    luaL_checktype(L, index, LUA_TTABLE);
    return SpriteLayout(lua::Stack::read<std::vector<SpriteLayout::Field>>(L, index), sprite, data);
}

void SpriteBatchLua::pushSpriteInstance(lua_State* L, const SpriteInstance& sprite, const PartColors& colors, const SpriteData& data) {
    lua_createtable(L, 0, 16);
    lua::Stack::push(L, sprite.position.x);
    lua_setfield(L, -2, "x");
    lua::Stack::push(L, sprite.position.y);
    lua_setfield(L, -2, "y");
    lua::Stack::push(L, sprite.size.x);
    lua_setfield(L, -2, "width");
    lua::Stack::push(L, sprite.size.y);
    lua_setfield(L, -2, "height");
    lua::Stack::push(L, sprite.source);
    lua_setfield(L, -2, "source");
    lua::Stack::push(L, sprite.pivot.x);
    lua_setfield(L, -2, "pivotX");
    lua::Stack::push(L, sprite.pivot.y);
    lua_setfield(L, -2, "pivotY");
    lua::Stack::push(L, sprite.rotation);
    lua_setfield(L, -2, "rotation");
    lua::Stack::push(L, sprite.color);
    lua_setfield(L, -2, "color");
    lua::Stack::push(L, sprite.flash);
    lua_setfield(L, -2, "flash");
    lua::Stack::push(L, sprite.flip.horizontal);
    lua_setfield(L, -2, "flipHorizontal");
    lua::Stack::push(L, sprite.flip.vertical);
    lua_setfield(L, -2, "flipVertical");
    lua::Stack::push(L, sprite.flip.diagonal);
    lua_setfield(L, -2, "flipDiagonal");
    lua::Stack::push(L, colors);
    lua_setfield(L, -2, "partColors");
    lua::Stack::push(L, static_cast<int>(data.texture) + 1);
    lua_setfield(L, -2, "texture");
    lua::Stack::push(L, std::vector<float>(data.values.begin(), data.values.end()));
    lua_setfield(L, -2, "data");
}

void SpriteBatchLua::readPartColors(lua_State* L, int table, SpriteBatch& batch, std::size_t index) {
    if (lua_getfield(L, table, "partColors") != LUA_TNIL) {
        batch.setPartColors(index, lua::Stack::read<PartColors>(L, -1));
    }
    lua_pop(L, 1);
}

std::size_t SpriteBatchLua::checkIndex(lua_State* L, int index, const SpriteBatch& batch) {
    const lua_Integer value = luaL_checkinteger(L, index);
    luaL_argcheck(L, value >= 1 && static_cast<std::size_t>(value) <= batch.size(), index, "sprite index out of range");
    return static_cast<std::size_t>(value - 1);
}

std::size_t SpriteBatchLua::checkFirst(lua_State* L, int index) {
    const lua_Integer first = luaL_optinteger(L, index, 1);
    luaL_argcheck(L, first >= 1, index, "sprites are numbered from one");
    return static_cast<std::size_t>(first - 1);
}

// A sprite without a size takes the size of the texture it draws from.
int SpriteBatchLua::add(lua_State* L) {
    SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    const std::optional<SpriteData> data = readSpriteData(L, 2, batch.getTextures().size());
    const graphics::Texture& texture = batch.getTextures()[data ? data->texture : 0];
    const std::size_t index = batch.add(lua::TypeConverter::readSpriteInstance(L, 2, texture, {}, {kPartColorFields, kDataFields}));
    readPartColors(L, 2, batch, index);
    if (data) {
        batch.setData(index, *data);
    }
    lua::Stack::push(L, index + 1);
    return 1;
}

int SpriteBatchLua::set(lua_State* L) {
    SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    const std::size_t index = checkIndex(L, 2, batch);
    luaL_checktype(L, 3, LUA_TTABLE);
    const std::optional<SpriteData> data = readSpriteData(L, 3, batch.getTextures().size(), batch.getData(index));
    const graphics::Texture& texture = batch.getTextures()[data ? data->texture : batch.getData(index).texture];
    batch.set(index, lua::TypeConverter::readSpriteInstance(L, 3, texture, batch.get(index), {kPartColorFields, kDataFields}));
    readPartColors(L, 3, batch, index);
    if (data) {
        batch.setData(index, *data);
    }
    return 0;
}

int SpriteBatchLua::get(lua_State* L) {
    const SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    const std::size_t index = checkIndex(L, 2, batch);
    pushSpriteInstance(L, batch.get(index), batch.getPartColors(index), batch.getData(index));
    return 1;
}

int SpriteBatchLua::reserve(lua_State* L) {
    lua::Userdata::check<SpriteBatch>(L, 1).reserve(lua::Stack::read<std::size_t>(L, 2));
    return 0;
}

// Sets the number of sprites with `resize(count[, sprite])`, where new sprites copy the sprite table.
int SpriteBatchLua::resize(lua_State* L) {
    SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    const lua_Integer count = luaL_checkinteger(L, 2);
    luaL_argcheck(L, count >= 0, 2, "the count cannot be negative");
    SpriteInstance sprite{.size = batch.getTexture().getSize()};
    if (!lua_isnoneornil(L, 3)) {
        sprite = lua::TypeConverter::readSpriteInstance(L, 3, batch.getTexture());
    }
    batch.resize(static_cast<std::size_t>(count), sprite);
    return 0;
}

// Copies the fields a buffer holds into the sprites with `writeFields(buffer, {'x', 'y', ...}[, first])`.
int SpriteBatchLua::writeFields(lua_State* L) {
    SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    const core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 2);
    const SpriteLayout layout = readLayout(L, 3);
    batch.writeFields(buffer.getValues(), layout, checkFirst(L, 4));
    return 0;
}

// Copies the fields of the sprites into a buffer with `readFields(buffer, {'x', 'y', ...}[, first])`.
int SpriteBatchLua::readFields(lua_State* L) {
    const SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    core::FloatBuffer& buffer = lua::Userdata::check<core::FloatBuffer>(L, 2);
    const SpriteLayout layout = readLayout(L, 3);
    batch.readFields(buffer.getValues(), layout, checkFirst(L, 4));
    return 0;
}

int SpriteBatchLua::remove(lua_State* L) {
    SpriteBatch& batch = lua::Userdata::check<SpriteBatch>(L, 1);
    batch.remove(checkIndex(L, 2, batch));
    return 0;
}

int SpriteBatchLua::clear(lua_State* L) {
    lua::Userdata::check<SpriteBatch>(L, 1).clear();
    return 0;
}

int SpriteBatchLua::size(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteBatch>(L, 1).size());
    return 1;
}

int SpriteBatchLua::draw(lua_State* L) {
    lua::Userdata::check<SpriteBatch>(L, 1).draw(lua::Runtime::getEngine(L).getRenderer2D(), lua::TypeConverter::readDrawOrder(L, 2));
    return 0;
}

int SpriteBatchLua::bake(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<SpriteBatch>(L, 1).bake(lua::Runtime::getEngine(L).getRenderer2D()));
    return 1;
}

std::size_t SpriteBatchLua::staticSize(const StaticSpriteBatch& batch) {
    return batch.size();
}

math::Rect SpriteBatchLua::staticBounds(const StaticSpriteBatch& batch) {
    return batch.getBounds();
}

graphics::Texture SpriteBatchLua::staticTexture(const StaticSpriteBatch& batch) {
    return batch.getTexture();
}

void SpriteBatchLua::install(lua_State* L) {
    lua::ClassBuilder<StaticSpriteBatch>(L).function("size", &lua::Binding::function<&staticSize>).function("bounds", &lua::Binding::function<&staticBounds>).function("texture", &lua::Binding::function<&staticTexture>).install();
    lua::ClassBuilder<SpriteBatch>(L).function("add", &lua::Binding::native<&add>).function("set", &lua::Binding::native<&set>).function("get", &lua::Binding::native<&get>).function("reserve", &lua::Binding::native<&reserve>).function("resize", &lua::Binding::native<&resize>).function("writeFields", &lua::Binding::native<&writeFields>).function("readFields", &lua::Binding::native<&readFields>).function("remove", &lua::Binding::native<&remove>).function("clear", &clear).function("size", &size).function("draw", &lua::Binding::native<&draw>).function("bake", &lua::Binding::native<&bake>).install();
}

} // namespace haylen::graphics2d
