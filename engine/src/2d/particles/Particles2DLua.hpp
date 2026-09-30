#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/math/FloatRange.hpp"

struct lua_State;

namespace haylen::particles2d {

struct Effect;

// Installs `haylen.particles2d` with the `ParticleEmitter` and `ParticleEffect` classes. Emitter options and effect files share the same fields.
class Particles2DLua final {
  public:
    static void install(lua_State* L);

    // Pushes a loaded effect, which the asset loader of particle effects hands to Lua.
    static void pushEffect(lua_State* L, std::shared_ptr<Effect> effect);

  private:
    static constexpr std::array<std::string_view, 26> kConfigFields{"texture", "frames", "rate", "bursts", "duration", "loop", "prewarm", "maxParticles", "lifetime", "speed", "direction", "spread", "gravity", "radialAcceleration", "tangentialAcceleration", "damping", "startSize", "endSize", "spin", "colors", "shape", "shapeSize", "localSpace", "layer", "depth", "blend"};
    static constexpr std::array<std::string_view, 1> kSeedFields{"seed"};
    static constexpr std::array<std::string_view, 2> kBurstFields{"time", "count"};

    static void readRange(lua_State* L, int table, const char* name, math::FloatRange& range);
    static void readBursts(lua_State* L, int table, std::vector<EmitterConfig::Burst>& bursts);
    [[nodiscard]] static EmitterConfig readConfig(lua_State* L, int index, EmitterConfig config, lua::Table::FieldNames extraFields);
    static void pushRange(lua_State* L, const char* name, math::FloatRange range);
    template <typename T> static void setValue(lua_State* L, const char* name, const T& value);
    static void pushConfig(lua_State* L, const EmitterConfig& config);

    static int newEmitter(lua_State* L);
    static int emitterConfigure(lua_State* L);
    static int emitterConfig(lua_State* L);
    static int emitterPositions(lua_State* L);
    static int emitterReadPositions(lua_State* L);
    static int emitterUpdate(lua_State* L);
    static int emitterRestart(lua_State* L);
    static int emitterCycleTime(lua_State* L);
    static int emitterDraw(lua_State* L);
    static int emitterBurst(lua_State* L);
    static int emitterClear(lua_State* L);
    static int emitterCount(lua_State* L);
    static int emitterAlive(lua_State* L);
    static int effectTexturePath(lua_State* L);
    static int effectConfig(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::particles2d
