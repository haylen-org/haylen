#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "haylen/3d/navigation/Crowd.hpp"
#include "haylen/3d/navigation/NavMesh.hpp"
#include "haylen/3d/navigation/QueryFilter.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua::navigation3d {

// A crowd of Lua with the mesh it walks, which it keeps alive, and the filters its agents named, in the order of the filters of the crowd.
struct ScriptedCrowd {
    std::shared_ptr<haylen::navigation3d::NavMesh> mesh;
    std::unique_ptr<haylen::navigation3d::Crowd> crowd;
    std::vector<haylen::navigation3d::QueryFilter> filters;
};

} // namespace haylen::lua::navigation3d

namespace haylen::lua {

template <> struct Type<lua::navigation3d::ScriptedCrowd> {
    static constexpr const char* name = "haylen.Crowd3D";
    using Storage = lua::navigation3d::ScriptedCrowd;
};

} // namespace haylen::lua

namespace haylen::lua::navigation3d {

// Installs the `Crowd3D` class: agents that walk a navigation mesh together, with reads and writes of every agent at once in float buffers.
class CrowdBinding final {
  public:
    static void install(lua_State* L);

    // Creates a crowd with `newCrowd(mesh[, options])`.
    static int create(lua_State* L);

  private:
    [[nodiscard]] static ScriptedCrowd& checkSelf(lua_State* L);
    [[nodiscard]] static haylen::navigation3d::Crowd::AgentId checkAgent(lua_State* L, int index);
    [[nodiscard]] static std::size_t findFilter(ScriptedCrowd& self, const haylen::navigation3d::QueryFilter& filter);
    [[nodiscard]] static haylen::navigation3d::Crowd::AgentOptions readAgent(lua_State* L, ScriptedCrowd& self, int index);

    static int addAgent(lua_State* L);
    static int removeAgent(lua_State* L);
    static int hasAgent(lua_State* L);
    static int setTarget(lua_State* L);
    static int setTargets(lua_State* L);
    static int setVelocity(lua_State* L);
    static int clearTarget(lua_State* L);
    static int setPosition(lua_State* L);
    static int completeLink(lua_State* L);
    static int step(lua_State* L);
    static int position(lua_State* L);
    static int velocity(lua_State* L);
    static int state(lua_State* L);
    static int partial(lua_State* L);
    static int target(lua_State* L);
    static int readPositions(lua_State* L);
    static int readVelocities(lua_State* L);
    static int linkEntries(lua_State* L);
    static int agents(lua_State* L);
    static int count(lua_State* L);
};

} // namespace haylen::lua::navigation3d
