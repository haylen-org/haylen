#pragma once

#include <array>
#include <string_view>
#include <vector>

#include "haylen/2d/physics/Explosion.hpp"

struct lua_State;

namespace haylen::physics2d {

// Installs the Terrain class of haylen.physics2d with explosions and fractures, which carve and break the bodies of a world.
class DestructionLua final {
  public:
    static void install(lua_State* L);

    // Sets newTerrain, explode, fracture and splitPolygon on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 12> kTerrainFields{"columns", "rows", "cellSize", "x", "y", "chunkSize", "simplifyTolerance", "friction", "restitution", "category", "mask", "group"};
    static constexpr std::array<std::string_view, 8> kExplosionFields{"x", "y", "radius", "impulse", "falloff", "occlusion", "category", "mask"};
    static constexpr std::array<std::string_view, 4> kFractureFields{"pieces", "impact", "seed", "minimumArea"};

    // Reads the fields of an explosion table over the given options.
    [[nodiscard]] static Explosion::Options readExplosion(lua_State* L, int index, Explosion::Options options);
    // Pushes {body, x, y, impulseX, impulseY} for each hit, with bodies of the Lua world at worldIndex.
    static void pushHits(lua_State* L, int worldIndex, const std::vector<Explosion::Hit>& hits);

    static int newTerrain(lua_State* L);
    static int terrainGetSamples(lua_State* L);
    static int terrainSetSamples(lua_State* L);
    static int terrainSample(lua_State* L);
    static int terrainIsSolid(lua_State* L);
    static int terrainFill(lua_State* L);
    static int terrainCarve(lua_State* L);
    static int terrainFillPolygon(lua_State* L);
    static int terrainCarvePolygon(lua_State* L);
    static int terrainExplode(lua_State* L);
    static int terrainUpdate(lua_State* L);
    static int terrainOutlines(lua_State* L);
    static int terrainBodies(lua_State* L);
    static int terrainColumns(lua_State* L);
    static int terrainRows(lua_State* L);
    static int terrainCellSize(lua_State* L);
    static int terrainBounds(lua_State* L);
    static int terrainChunkCount(lua_State* L);
    static int terrainDirtyChunkCount(lua_State* L);

    static int explode(lua_State* L);
    static int fracture(lua_State* L);
    static int splitPolygon(lua_State* L);
};

} // namespace haylen::physics2d
