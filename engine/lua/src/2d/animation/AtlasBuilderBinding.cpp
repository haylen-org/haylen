#include "2d/animation/AtlasBuilderBinding.hpp"

#include <lua.hpp>

#include <cmath>
#include <optional>
#include <string>
#include <utility>

#include "graphics/ImageBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "varn/async/Promise.h"

namespace haylen::lua::animation2d {

int AtlasBuilderBinding::create(lua_State* L) {
    lua::Userdata::emplace<haylen::animation2d::AtlasBuilder>(L, std::make_shared<haylen::animation2d::AtlasBuilder>());
    return 1;
}

haylen::animation2d::AtlasBuilder::Options AtlasBuilderBinding::readOptions(lua_State* L, int index) {
    haylen::animation2d::AtlasBuilder::Options options;
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
int AtlasBuilderBinding::add(lua_State* L) {
    haylen::animation2d::AtlasBuilder& builder = lua::Userdata::check<haylen::animation2d::AtlasBuilder>(L, 1);
    std::string name(lua::Stack::read<std::string_view>(L, 2));
    const std::shared_ptr<haylen::graphics::Image> image = lua::Userdata::checkShared<haylen::graphics::Image>(L, 3);
    std::optional<haylen::graphics::Image::Area> area;
    if (!lua_isnoneornil(L, 4)) {
        const auto rect = lua::Stack::read<haylen::math::Rect>(L, 4);
        area = haylen::graphics::Image::Area{static_cast<int>(std::lround(rect.x)), static_cast<int>(std::lround(rect.y)), static_cast<int>(std::lround(rect.width)), static_cast<int>(std::lround(rect.height))};
    }

    builder.add(std::move(name), image, area);
    return 0;
}

// Adds the frames and animations of an atlas with `addAtlas(prefix, atlas, image)`, where the image holds the pixels of the texture of the atlas.
int AtlasBuilderBinding::addAtlas(lua_State* L) {
    lua::Userdata::check<haylen::animation2d::AtlasBuilder>(L, 1).addAtlas(lua::Stack::read<std::string_view>(L, 2), lua::Userdata::check<haylen::animation2d::SpriteAtlas>(L, 3), lua::Userdata::checkShared<haylen::graphics::Image>(L, 4));
    return 0;
}

int AtlasBuilderBinding::build(lua_State* L) {
    const haylen::animation2d::AtlasBuilder::Options options = readOptions(L, 2);
    haylen::animation2d::AtlasBuilder::Packed packed = lua::Userdata::check<haylen::animation2d::AtlasBuilder>(L, 1).pack(options);
    haylen::graphics::Texture texture = lua::Runtime::getEngine(L).getGraphics().createTexture(packed.image, options.texture);
    lua::Stack::push(L, std::make_shared<haylen::animation2d::SpriteAtlas>(haylen::animation2d::AtlasBuilder::finish(std::move(packed), std::move(texture))));
    return 1;
}

// Packs a copy of the builder on the task pool and makes the texture on the frame thread, so adding more to the builder meanwhile changes nothing of this atlas.
int AtlasBuilderBinding::buildAsync(lua_State* L) {
    const haylen::animation2d::AtlasBuilder::Options options = readOptions(L, 2);
    auto builder = std::make_shared<const haylen::animation2d::AtlasBuilder>(lua::Userdata::check<haylen::animation2d::AtlasBuilder>(L, 1));
    haylen::core::Engine& engine = lua::Runtime::getEngine(L);
    auto promise = std::make_shared<varn::async::Promise>(engine.getPlugin<lua::ScriptPlugin>().getRuntime());
    // clang-format off
    engine.getJobs().run([builder, options] { return builder->pack(options); }, [promise, device = &engine.getGraphics(), options](haylen::core::JobSystem::Result<haylen::animation2d::AtlasBuilder::Packed> result) {
        if (!result.isOk()) {
            promise->reject(result.error);
            return;
        }

        std::shared_ptr<haylen::animation2d::SpriteAtlas> atlas;
        try {
            haylen::graphics::Texture texture = device->createTexture(result.value->image, options.texture);
            atlas = std::make_shared<haylen::animation2d::SpriteAtlas>(haylen::animation2d::AtlasBuilder::finish(std::move(*result.value), std::move(texture)));
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

int AtlasBuilderBinding::count(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::animation2d::AtlasBuilder>(L, 1).size());
    return 1;
}

void AtlasBuilderBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::animation2d::AtlasBuilder>(L).function("add", &lua::Binding::native<&add>).function("addAtlas", &lua::Binding::native<&addAtlas>).function("build", &lua::Binding::native<&build>).function("buildAsync", &lua::Binding::native<&buildAsync>).property("count", &count).install();
}

} // namespace haylen::lua::animation2d
