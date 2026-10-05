#include "haylen/plugins/AssetsPlugin.hpp"

#include <stdexcept>
#include <utility>

#include "assets/AssetsLua.hpp"
#include "graphics/ShaderResource.hpp"
#include "graphics/TextureResource.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"

namespace haylen::plugins {

AssetsPlugin::AssetsPlugin() {
    // clang-format off
    registerLuaPusher("texture", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, graphics::Texture(std::static_pointer_cast<graphics::TextureResource>(asset)));
    });
    registerLuaPusher("font", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, std::static_pointer_cast<text::Font>(asset));
    });
    registerLuaPusher("shader", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, graphics::Shader(std::static_pointer_cast<graphics::ShaderResource>(asset)));
    });
    registerLuaPusher("vectorImage", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::Stack::push(L, graphics::VectorImage(std::static_pointer_cast<graphics::VectorImageResource>(asset)));
    });
    registerLuaPusher("json", [](lua_State* L, const std::shared_ptr<void>& asset) {
        lua::JsonConverter::push(L, *std::static_pointer_cast<core::Json>(asset));
    });
    // clang-format on
}

void AssetsPlugin::installLua(core::Engine&, lua_State* L) {
    assets::AssetsLua::install(L);
}

void AssetsPlugin::registerLuaPusher(std::string type, LuaPusher pusher) {
    pushers.insert_or_assign(std::move(type), std::move(pusher));
}

void AssetsPlugin::pushAsset(lua_State* L, std::string_view type, const std::shared_ptr<void>& asset) const {
    const auto found = pushers.find(std::string(type));
    if (found == pushers.end()) {
        throw std::logic_error("The asset type \"" + std::string(type) + "\" has no Lua representation.");
    }
    found->second(L, asset);
}

} // namespace haylen::plugins
