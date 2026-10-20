#pragma once

#include <lua.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include "haylen/content/PackSource.hpp"
#include "lua/Owners.hpp"
#include "lua/WeakReference.hpp"

namespace haylen::content {

// A source of packs that a Lua function is, such as the one of a store plugin written in Lua. The function runs as a task of its owner with the name of a pack and a progress function, which takes a table with `receivedBytes` and `totalBytes`, and returns the folder that holds the shards of the delivered pack, `nil` when it does not deliver the pack, or a promise of either. An error of the function or a rejected promise fails the load that needs the pack.
class LuaPackSource final : public PackSource {
  public:
    // An owner index of 0 means no owner.
    LuaPackSource(lua_State* L, int functionIndex, int ownerIndex);

    void acquire(const std::string& pack, std::function<void(const Progress&)> progress, Delivery done) override;

  private:
    static constexpr const char* kRequestType = "haylen.PackRequest";
    static constexpr const char* kProgressType = "haylen.PackProgress";
    static constexpr lua_KContext kCalled = 0;
    static constexpr lua_KContext kAwaited = 1;

    // The delivery of one request, which answers once, whether the function returns, fails or its promise settles.
    struct Answer {
        Delivery done;

        void deliver(std::optional<std::filesystem::path> folder, std::optional<Error> failure);
    };

    static int run(lua_State* L);
    static int continueRun(lua_State* L, int status, lua_KContext context);
    static int reportProgress(lua_State* L);
    template <typename T> static int collect(lua_State* L);

    lua_State* main;
    std::shared_ptr<lua::Owners::Function> function;
    std::shared_ptr<lua::WeakReference> owner;
};

} // namespace haylen::content
