#include "graphics/ImageLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <string_view>
#include <utility>

#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::graphics {

void ImageLua::push(lua_State* L, Image image) {
    lua::Userdata::emplace<Image>(L, std::make_shared<Image>(std::move(image)));
}

Image& ImageLua::checkWritable(lua_State* L, int index) {
    auto& storage = *static_cast<lua::Type<Image>::Storage*>(luaL_checkudata(L, index, lua::Type<Image>::name));
    if (storage.use_count() > 1) {
        storage = std::make_shared<Image>(*storage);
    }
    return *storage;
}

// Reads a rectangle into whole pixels, or the whole image for `nil`.
Image::Area ImageLua::readArea(lua_State* L, int index, const Image& image) {
    if (lua_isnoneornil(L, index)) {
        return image.getArea();
    }
    const auto area = lua::Stack::read<math::Rect>(L, index);
    return {static_cast<int>(std::lround(area.x)), static_cast<int>(std::lround(area.y)), static_cast<int>(std::lround(area.width)), static_cast<int>(std::lround(area.height))};
}

int ImageLua::create(lua_State* L) {
    const auto columns = static_cast<int>(luaL_checkinteger(L, 1));
    const auto rows = static_cast<int>(luaL_checkinteger(L, 2));
    if (columns < 1 || rows < 1) {
        throw std::invalid_argument("An image needs a width and a height of one pixel or more.");
    }
    push(L, Image(columns, rows, lua_isnoneornil(L, 3) ? math::Color::transparent() : lua::Stack::read<math::Color>(L, 3)));
    return 1;
}

int ImageLua::decode(lua_State* L) {
    const auto bytes = lua::Stack::read<std::string_view>(L, 1);
    push(L, Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()}));
    return 1;
}

int ImageLua::decodeAsync(lua_State* L) {
    std::string bytes(lua::Stack::read<std::string_view>(L, 1));
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([bytes = std::move(bytes)] {
        return Image::decode({reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()});
    }, [promise](core::JobSystem::Result<Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        auto image = std::make_shared<Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::pixel(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getPixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3))));
    return 1;
}

int ImageLua::setPixel(lua_State* L) {
    checkWritable(L, 1).setPixel(static_cast<int>(luaL_checkinteger(L, 2)), static_cast<int>(luaL_checkinteger(L, 3)), lua::Stack::read<math::Color>(L, 4));
    return 0;
}

// Fills with `fill(color, rect)`, the whole image without a rectangle.
int ImageLua::fill(lua_State* L) {
    Image& image = checkWritable(L, 1);
    image.fill(readArea(L, 3, image), lua::Stack::read<math::Color>(L, 2));
    return 0;
}

// Draws another image into this one with `blit(source, x, y, {source = rect, blend = false})`.
int ImageLua::blit(lua_State* L) {
    const std::shared_ptr<Image> source = lua::Userdata::checkShared<Image>(L, 2);
    Image& image = checkWritable(L, 1);
    Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }
    image.blit(*source, from, static_cast<int>(luaL_checkinteger(L, 3)), static_cast<int>(luaL_checkinteger(L, 4)), blend ? Image::BlitMode::Blend : Image::BlitMode::Replace);
    return 0;
}

// Returns a promise of a copy of this image with the source drawn into it, both made on the task pool, while this image stays as it is.
int ImageLua::blitAsync(lua_State* L) {
    const std::shared_ptr<const Image> target = lua::Userdata::checkShared<Image>(L, 1);
    const std::shared_ptr<const Image> source = lua::Userdata::checkShared<Image>(L, 2);
    Image::Area from = source->getArea();
    bool blend = false;
    if (!lua_isnoneornil(L, 5)) {
        luaL_checktype(L, 5, LUA_TTABLE);
        lua::Table::checkFields(L, 5, {kBlitFields});
        lua_getfield(L, 5, "source");
        from = readArea(L, -1, *source);
        lua_pop(L, 1);
        lua::Table::readField(L, 5, "blend", blend);
    }
    const auto x = static_cast<int>(luaL_checkinteger(L, 3));
    const auto y = static_cast<int>(luaL_checkinteger(L, 4));
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([target, source, from, x, y, blend] {
        Image result = *target;
        result.blit(*source, from, x, y, blend ? Image::BlitMode::Blend : Image::BlitMode::Replace);
        return result;
    }, [promise](core::JobSystem::Result<Image> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        auto image = std::make_shared<Image>(std::move(*result.value));
        promise->resolveCustom([image](lua_State* state) { lua::Userdata::emplace<Image>(state, image); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int ImageLua::crop(lua_State* L) {
    const Image& image = lua::Userdata::check<Image>(L, 1);
    push(L, image.crop(readArea(L, 2, image)));
    return 1;
}

int ImageLua::bleed(lua_State* L) {
    checkWritable(L, 1).bleed();
    return 0;
}

int ImageLua::visibleArea(lua_State* L) {
    const Image::Area area = lua::Userdata::check<Image>(L, 1).getVisibleArea();
    if (area.width == 0) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, math::Rect{static_cast<float>(area.x), static_cast<float>(area.y), static_cast<float>(area.width), static_cast<float>(area.height)});
    return 1;
}

int ImageLua::clone(lua_State* L) {
    push(L, lua::Userdata::check<Image>(L, 1));
    return 1;
}

int ImageLua::width(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getWidth());
    return 1;
}

int ImageLua::height(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Image>(L, 1).getHeight());
    return 1;
}

void ImageLua::install(lua_State* L) {
    lua::ClassBuilder<Image>(L).function("pixel", &lua::Binding::native<&pixel>).function("setPixel", &lua::Binding::native<&setPixel>).function("fill", &lua::Binding::native<&fill>).function("blit", &lua::Binding::native<&blit>).function("blitAsync", &lua::Binding::native<&blitAsync>).function("crop", &lua::Binding::native<&crop>).function("bleed", &lua::Binding::native<&bleed>).function("visibleArea", &lua::Binding::native<&visibleArea>).function("clone", &lua::Binding::native<&clone>).property("width", &width).property("height", &height).install();
}

} // namespace haylen::graphics
