#pragma once

#include <lua.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "haylen/lua/Error.hpp"
#include "haylen/lua/Reference.hpp"

namespace haylen::io {
class Package;
}

namespace haylen::lua {

class ModuleClosures;
class ModuleGraph;

// Reloads changed Lua modules in place, one batch at a time. A module runs again in a staging environment with an owner of its own, and only when it ran without errors does its new code join the old state: the new functions take over the variables of the old ones by name, the new value merges into the live value field by field, and its globals merge into the globals. A literal the developer edited wins while the app never changed it, state the app changed wins otherwise, and engine objects and instances keep their identity. Once every module of the batch reloaded, one walk of the heap puts the new functions wherever the old ones were.
class ModuleReloader final {
  public:
    enum class Status : std::uint8_t {
        Patched,
        Unchanged,
        Failed,
        Restart,
    };

    // A failed module changed nothing and carries its error, and a module that cannot reload in place carries the reason the app restarts instead.
    struct Result {
        Status status = Status::Unchanged;
        std::optional<Error> error;
        std::string reason;
    };

    ModuleReloader(lua_State* state, ModuleGraph& modules, const io::Package& source);

    // Reloads the loaded module of the file. A batch reloads its modules in the order of `ModuleGraph::order`, so a module runs after the modules it requires.
    [[nodiscard]] Result reload(const std::string& path);

    // Puts the new functions and the live tables in place of the old functions and the staging tables everywhere in the heap, once the batch reloaded, and pushes a list of the tables whose metatable is a patched table, such as the instances of a patched class.
    void finish();

  private:
    [[nodiscard]] static std::string_view getKind(lua_State* L, int index) noexcept;

    // Leaves the value that the field keeps on the stack, from the value of the last load at `base`, the live value and the value of the new load.
    void pushMerged(int base, int live, int fresh, const std::string& field);
    void mergeTable(int live, int fresh, int base, const std::string& field);
    void mergeMetatables(int live, int fresh, const std::string& field);

    // Maps the functions of a live table that the new load replaced in the same table, such as a table kept with `hotReload.keep`, from the functions of the last load.
    void mapReplaced(int live, int base, int depth);
    void mapFunction(int previous, int fresh);
    void joinVariables(const ModuleClosures& previous, const ModuleClosures& fresh, int base);
    void mergeGlobals(int staging, int base);
    void pushPreviousGlobals(int base);
    [[nodiscard]] bool isClaimed(int index, bool byModule);
    [[nodiscard]] bool isOfModule(int function);

    lua_State* L;
    ModuleGraph& graph;
    const io::Package& package;
    Reference replacements;
    Reference patched;
    std::string path;
    std::string name;
};

} // namespace haylen::lua
