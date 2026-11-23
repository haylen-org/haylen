#include "haylen/lua/modules/AssetsModule.hpp"

#include <stdexcept>
#include <utility>

#include "assets/AssetsLua.hpp"
#include "graphics/ImageLua.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::modules {

// The fonts of the text plugin, bitmap and grid fonts among them, reach Lua as the fonts they are.
AssetsModule::AssetsModule() {
    // clang-format off
    registerPusher("texture", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(asset)));
    });
    registerPusher("image", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Userdata::emplace<graphics::Image>(L, std::static_pointer_cast<graphics::Image>(asset));
    });
    for (const char* type : {"font", "bitmapFont", "gridFont"}) {
        registerPusher(type, [](lua_State* L, const std::shared_ptr<void>& asset) {
            Stack::push(L, std::static_pointer_cast<text::Font>(asset));
        });
    }
    registerPusher("shader", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, graphics::Shader(std::static_pointer_cast<graphics::ShaderResource>(asset)));
    });
    registerPusher("vectorImage", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, graphics::VectorImage(std::static_pointer_cast<graphics::VectorImageResource>(asset)));
    });
    registerPusher("json", [](lua_State* L, const std::shared_ptr<void>& asset) {
        JsonConverter::push(L, *std::static_pointer_cast<core::Json>(asset));
    });
    // clang-format on
}

void AssetsModule::install(core::Engine&, lua_State* L) {
    assets::AssetsLua::install(L);
}

void AssetsModule::registerPusher(std::string type, Pusher pusher) {
    pushers.insert_or_assign(std::move(type), std::move(pusher));
}

void AssetsModule::pushAsset(lua_State* L, std::string_view type, const std::shared_ptr<void>& asset) const {
    const auto found = pushers.find(std::string(type));
    if (found == pushers.end()) {
        throw std::logic_error("The asset type \"" + std::string(type) + "\" has no Lua representation.");
    }

    found->second(L, asset);
}

} // namespace haylen::lua::modules
