#pragma once

#include <array>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/2d/particles/EmitterConfig.hpp"
#include "haylen/2d/particles/Trail.hpp"
#include "haylen/lua/Table.hpp"

struct lua_State;

namespace haylen::particles2d {

struct Effect;
class ImageShape;

// Installs `haylen.particles2d` with the `ParticleEmitter`, `ParticleEffect`, `ParticleSystem`, `Trail` and `ImageShape` classes. Emitter options and effect files share the same fields.
class Particles2DLua final {
  public:
    static void install(lua_State* L);

    // Pushes a loaded effect, which the asset loader of particle effects hands to Lua.
    static void pushEffect(lua_State* L, std::shared_ptr<Effect> effect);

    // Pushes a loaded image shape, which the asset loader of image shapes hands to Lua.
    static void pushImageShape(lua_State* L, std::shared_ptr<ImageShape> shape);

  private:
    static constexpr std::array<std::string_view, 70> kConfigFields{
        "texture", "frames", "frameGrid", "frameMode", "frameRate", "rate", "rateOverDistance", "bursts", "delay", "duration", "loop", "prewarm", "maxParticles", "lifetime", "speed", "speedCurve", "direction", "directionMode", "spread", "inheritVelocity", "gravity", "radialAcceleration", "tangentialAcceleration", "damping", "turbulence", "attractors", "collision", "bounds", "boundsMode", "startSize", "endSize", "endSizeScale", "sizeCurve", "aspect", "stretch", "rotation", "rotationStep", "alignToVelocity", "spin", "spinCurve", "colors", "colorTimes", "colorBlend", "tints", "tintMode", "shape", "shapeSize", "shapeAngle", "shapeArc", "shapePoints", "shapeThickness", "shapeImage", "colorFromImage", "localSpace", "pixelSnap", "particleOrder", "subEmitters", "trail", "light", "particleLights", "layer", "depth", "sortOffset", "visibility", "blend", "material", "emission", "unshaded", "lightMask", "distortion",
    };
    static constexpr std::array<std::string_view, 1> kSeedFields{"seed"};
    static constexpr std::array<std::string_view, 5> kBurstFields{"time", "count", "cycles", "interval", "probability"};
    static constexpr std::array<std::string_view, 3> kGridFields{"columns", "rows", "count"};
    static constexpr std::array<std::string_view, 3> kTurbulenceFields{"strength", "frequency", "speed"};
    static constexpr std::array<std::string_view, 6> kAttractorFields{"x", "y", "strength", "radius", "killRadius", "space"};
    static constexpr std::array<std::string_view, 7> kCollisionFields{"type", "y", "area", "bounce", "friction", "result", "lifeLoss"};
    static constexpr std::array<std::string_view, 8> kSubEmitterFields{"effect", "trigger", "count", "rate", "probability", "inheritVelocity", "inheritColor", "overrides"};
    static constexpr std::array<std::string_view, 6> kTrailFields{"length", "lifetime", "widthStart", "widthEnd", "colors", "texture"};
    static constexpr std::array<std::string_view, 7> kLightFields{"type", "offset", "radius", "color", "intensity", "flicker", "fade"};
    static constexpr std::array<std::string_view, 2> kFlickerFields{"speed", "amount"};
    static constexpr std::array<std::string_view, 3> kParticleLightFields{"radius", "intensity", "max"};
    static constexpr std::array<std::string_view, 2> kFilterFields{"category", "mask"};
    static constexpr std::array<std::string_view, 7> kTrailOptionFields{"lifetime", "minDistance", "maxPoints", "widthStart", "widthEnd", "colors", "texture"};
    static constexpr std::array<std::string_view, 9> kTrailOrderFields{"layer", "depth", "sortOffset", "visibility", "blend", "material", "emission", "unshaded", "lightMask"};

    [[nodiscard]] static EmitterConfig readConfig(lua_State* L, int index, EmitterConfig config, lua::Table::FieldNames extraFields);
    static void readKey(lua_State* L, std::string_view key, EmitterConfig& config);
    static void readSimulation(lua_State* L, std::string_view key, EmitterConfig& config);
    static void readLook(lua_State* L, std::string_view key, EmitterConfig& config);
    static void readBursts(lua_State* L, std::vector<EmitterConfig::Burst>& bursts);
    static void readAttractors(lua_State* L, std::vector<EmitterConfig::Attractor>& attractors);
    static void readCollision(lua_State* L, EmitterConfig::Collision& collision);
    static void readSubEmitters(lua_State* L, std::vector<EmitterConfig::SubEmitter>& subEmitters);
    static void readLight(lua_State* L, EmitterConfig& config);
    static void readParticleLights(lua_State* L, EmitterConfig& config);
    static void readTrail(lua_State* L, EmitterConfig::ParticleTrail& trail);
    static void readCurve(lua_State* L, std::string_view key, math::EasingCurve& curve);
    [[nodiscard]] static Trail::Options readTrailOptions(lua_State* L, int index, Trail::Options options);

    template <typename T> static void setValue(lua_State* L, const char* name, const T& value);
    static void pushConfig(lua_State* L, const EmitterConfig& config);
    static void pushMotion(lua_State* L, const EmitterConfig& config);
    static void pushLook(lua_State* L, const EmitterConfig& config);
    static void pushTrailOptions(lua_State* L, const Trail::Options& options);

    static int newEmitter(lua_State* L);
    static int newSystem(lua_State* L);
    static int newTrail(lua_State* L);
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
    static int emitterParticle(lua_State* L);
    static int emitterSetAttractor(lua_State* L);
    static int emitterSetShapePoints(lua_State* L);
    static int emitterSetCollisionWorld(lua_State* L);
    static int effectTexturePath(lua_State* L);
    static int effectConfig(lua_State* L);
    static int effectComposite(lua_State* L);
    static int effectParts(lua_State* L);
    static int systemUpdate(lua_State* L);
    static int systemDraw(lua_State* L);
    static int systemRestart(lua_State* L);
    static int systemClear(lua_State* L);
    static int systemEmitter(lua_State* L);
    static int systemEmitters(lua_State* L);
    static int systemCount(lua_State* L);
    static int systemAlive(lua_State* L);
    static int systemEmitting(lua_State* L);
    static int systemSetEmitting(lua_State* L);
    static int trailUpdate(lua_State* L);
    static int trailDraw(lua_State* L);
    static int trailClear(lua_State* L);
    static int trailConfigure(lua_State* L);
    static int trailConfig(lua_State* L);
    static int trailCount(lua_State* L);
    static int trailAlive(lua_State* L);
    static int shapeCount(lua_State* L);
    static int shapeWidth(lua_State* L);
    static int shapeHeight(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::particles2d
