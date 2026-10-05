#pragma once

#include <lua.hpp>

#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Reference.hpp"

namespace haylen::lua {

// The Lua modules of an app in development: the names and the source text each package module loaded with, which modules require which, and, in a table of the registry, the owner of what each module registered while it ran its top level, the values it kept with `hotReload.keep` and the snapshot of what it produced. It replaces `require` with a version that records all of this, which only development uses, so the modules of apps that ship load exactly as before.
class ModuleGraph final {
  public:
    struct Module {
        std::string path;
        std::vector<std::string> names;
        std::string source;

        // The package paths of the modules that this one required, at its top level or later.
        std::set<std::string> dependencies;
        bool restartOnChange = false;
    };

    // Replaces `require` in the Lua state, whose modules are then recorded here. The graph must outlive the Lua state.
    void install(lua_State* L);

    [[nodiscard]] const Module* find(std::string_view path) const;
    [[nodiscard]] Module* find(std::string_view path);

    // Returns the paths in an order where every module comes after the modules it requires, keeping the load order inside a cycle.
    [[nodiscard]] std::vector<std::string> order(const std::vector<std::string>& paths) const;

    // Returns the loaded modules that require any of the paths, directly or through other modules, in load order.
    [[nodiscard]] std::vector<std::string> findDependents(const std::vector<std::string>& paths) const;

    // The path of the module that runs its top level now, or an empty text.
    [[nodiscard]] std::string_view getLoading() const noexcept;

    // Starts and ends a run of the top level of a module, during which registrations without an owner belong to the owner at index and `hotReload.keep` reads the kept values of the module. Runs nest like requires do.
    void beginLoad(lua_State* L, const std::string& path, int owner);
    void endLoad(lua_State* L);

    // Push the owner, the snapshot and the kept values of a module, or `nil`.
    static void pushOwner(lua_State* L, std::string_view path);
    static void pushSnapshot(lua_State* L, std::string_view path);
    static void pushKept(lua_State* L, std::string_view path);

    // Set the owner and the snapshot of a module from the values at the indices.
    static void setOwner(lua_State* L, std::string_view path, int index);
    static void setSnapshot(lua_State* L, std::string_view path, int index);

    // Pushes the table of the registry that holds the owners, snapshots and kept values, which a walk of the heap leaves alone.
    static void pushStore(lua_State* L);

    // Pushes the weak table that maps every table a module produced when it loaded to the path of that module.
    static void pushCreators(lua_State* L);

    // Gives the chunk at index the value on top of the stack as its `_ENV`, which every function of the chunk shares, and pops it.
    static void setEnvironment(lua_State* L, int chunk);

  private:
    static constexpr const char* kStore = "haylen.modules";
    static constexpr const char* kLoadType = "haylen.ModuleLoad";

    // A first load in progress, closed when the top level of the module returns or fails, whose user value holds the chunk.
    struct Load {
        ModuleGraph* graph = nullptr;
        bool finished = false;
    };

    struct Loading {
        std::string path;
        Reference owner;
    };

    static void pushSection(lua_State* L, const char* name);
    static int require(lua_State* L);
    static int recordGlobal(lua_State* L);
    static int closeLoad(lua_State* L);

    // Finds the loader of a module the way `require` does, leaving the loader and its data on the stack.
    static void findLoader(lua_State* L, const char* name);
    void recordRequire(lua_State* L, const std::string& name);
    int load(lua_State* L, const std::string& name, const std::string& path);
    void finishLoad(lua_State* L, const std::string& path, int result, int written);

    std::map<std::string, Module, std::less<>> modules;
    std::map<std::string, std::string, std::less<>> pathsByName;
    std::vector<std::string> loadOrder;
    std::vector<Loading> loading;
};

} // namespace haylen::lua
