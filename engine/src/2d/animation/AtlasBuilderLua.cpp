#include "2d/animation/AtlasBuilderLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "graphics/ImageLua.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::animation2d {

int AtlasBuilderLua::create(lua_State* L) {
    lua::Userdata::emplace<AtlasBuilder>(L, std::make_shared<AtlasBuilder>());
    return 1;
}

AtlasBuilder::Options AtlasBuilderLua::readOptions(lua_State* L, int index) {
    AtlasBuilder::Options options;
    if (lua_isnoneornil(L, index)) {
        return options;
    }
    options.texture = lua::TypeConverter::readTextureOptions(L, index, {kOptionFields});
    lua::Table::readField(L, index, "padding", options.padding);
    lua::Table::readField(L, index, "extrude", options.extrude);
    lua::Table::readField(L, index, "maxSize", options.maxSize);
    lua::Table::readField(L, index, "trim", options.trim);
    options.maxSize = std::min(options.maxSize, lua::Runtime::getEngine(L).getGraphics().getMaxTextureSize());
    return options;
}

// Adds a frame with `add(name, image, rect)`, the whole image without a rectangle.
int AtlasBuilderLua::add(lua_State* L) {
    AtlasBuilder& builder = lua::Userdata::check<AtlasBuilder>(L, 1);
    std::string name(lua::Stack::read<std::string_view>(L, 2));
    const std::shared_ptr<graphics::Image> image = lua::Userdata::checkShared<graphics::Image>(L, 3);
    std::optional<graphics::Image::Area> area;
    if (!lua_isnoneornil(L, 4)) {
        const auto rect = lua::Stack::read<math::Rect>(L, 4);
        area = graphics::Image::Area{static_cast<int>(std::lround(rect.x)), static_cast<int>(std::lround(rect.y)), static_cast<int>(std::lround(rect.width)), static_cast<int>(std::lround(rect.height))};
    }
    builder.add(std::move(name), image, area);
    return 0;
}

// Adds the frames and animations of an atlas with `addAtlas(prefix, atlas, image)`, where the image holds the pixels of the texture of the atlas.
int AtlasBuilderLua::addAtlas(lua_State* L) {
    lua::Userdata::check<AtlasBuilder>(L, 1).addAtlas(lua::Stack::read<std::string_view>(L, 2), lua::Userdata::check<SpriteAtlas>(L, 3), lua::Userdata::checkShared<graphics::Image>(L, 4));
    return 0;
}

int AtlasBuilderLua::build(lua_State* L) {
    const AtlasBuilder::Options options = readOptions(L, 2);
    AtlasBuilder::Packed packed = lua::Userdata::check<AtlasBuilder>(L, 1).pack(options);
    graphics::Texture texture = lua::Runtime::getEngine(L).getGraphics().createTexture(packed.image, options.texture);
    lua::Stack::push(L, std::make_shared<SpriteAtlas>(AtlasBuilder::finish(std::move(packed), std::move(texture))));
    return 1;
}

// Packs a copy of the builder on the task pool and makes the texture on the frame thread, so adding more to the builder meanwhile changes nothing of this atlas.
int AtlasBuilderLua::buildAsync(lua_State* L) {
    const AtlasBuilder::Options options = readOptions(L, 2);
    auto builder = std::make_shared<const AtlasBuilder>(lua::Userdata::check<AtlasBuilder>(L, 1));
    core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getScriptRuntime());
    // clang-format off
    engine.getJobs().run([builder, options] { return builder->pack(options); }, [promise, device = &engine.getGraphics(), options](core::JobSystem::Result<AtlasBuilder::Packed> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }
        std::shared_ptr<SpriteAtlas> atlas;
        try {
            graphics::Texture texture = device->createTexture(result.value->image, options.texture);
            atlas = std::make_shared<SpriteAtlas>(AtlasBuilder::finish(std::move(*result.value), std::move(texture)));
        } catch (const std::exception& error) {
            promise->reject(error.what());
            return;
        }
        promise->resolveCustom([atlas](lua_State* state) { lua::Stack::push(state, atlas); });
    });
    // clang-format on
    varn::async::Promise::push(L, promise);
    return 1;
}

int AtlasBuilderLua::count(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<AtlasBuilder>(L, 1).size());
    return 1;
}

void AtlasBuilderLua::install(lua_State* L) {
    lua::ClassBuilder<AtlasBuilder>(L).function("add", &lua::Binding::native<&add>).function("addAtlas", &lua::Binding::native<&addAtlas>).function("build", &lua::Binding::native<&build>).function("buildAsync", &lua::Binding::native<&buildAsync>).property("count", &count).install();
}

} // namespace haylen::animation2d
