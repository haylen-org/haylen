#include "haylen/lua/modules/AssetsModule.hpp"

#include <stdexcept>
#include <utility>

#include "assets/AssetsBinding.hpp"
#include "graphics/ImageBinding.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::modules {

// The fonts of the text plugin, bitmap and grid fonts among them, reach Lua as the fonts they are.
AssetsModule::AssetsModule() {
    // clang-format off
    registerPusher("texture", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, haylen::graphics::Texture(std::static_pointer_cast<haylen::graphics::TextureResource>(asset)));
    });
    registerPusher("image", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Userdata::emplace<haylen::graphics::Image>(L, std::static_pointer_cast<haylen::graphics::Image>(asset));
    });
    for (const char* type : {"font", "bitmapFont", "gridFont"}) {
        registerPusher(type, [](lua_State* L, const std::shared_ptr<void>& asset) {
            Stack::push(L, std::static_pointer_cast<haylen::text::Font>(asset));
        });
    }
    registerPusher("shader", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, haylen::graphics::Shader(std::static_pointer_cast<haylen::graphics::ShaderResource>(asset)));
    });
    registerPusher("vectorImage", [](lua_State* L, const std::shared_ptr<void>& asset) {
        Stack::push(L, haylen::graphics::VectorImage(std::static_pointer_cast<haylen::graphics::VectorImageResource>(asset)));
    });
    registerPusher("json", [](lua_State* L, const std::shared_ptr<void>& asset) {
        JsonConverter::push(L, *std::static_pointer_cast<haylen::core::Json>(asset));
    });
    // clang-format on
}

void AssetsModule::install(haylen::core::Engine&, lua_State* L) {
    lua::assets::AssetsBinding::install(L);
}

void AssetsModule::registerPusher(std::string type, Pusher pusher) {
    pushers.insert_or_assign(std::move(type), std::move(pusher));
}

void AssetsModule::pushAsset(lua_State* L, std::string_view type, const std::shared_ptr<void>& asset) const {
    const auto found = pushers.find(std::string(type));
    if (found == pushers.end()) {
        throw std::logic_error("The asset type \"" + std::string(type) + "\" loads, but no Lua module of the app gives it a form in Lua. Add a module that gives assets of this type a form in Lua.");
    }

    found->second(L, asset);
}

} // namespace haylen::lua::modules
