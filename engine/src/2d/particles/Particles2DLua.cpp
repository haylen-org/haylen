#include "2d/particles/Particles2DLua.hpp"

#include <algorithm>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include "2d/lighting/LightLua.hpp"
#include "2d/physics/Physics2DLua.hpp"
#include "2d/physics/ShapeLua.hpp"
#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/Emitter.hpp"
#include "haylen/2d/particles/ImageShape.hpp"
#include "haylen/2d/particles/System.hpp"
#include "haylen/2d/particles/Trail.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<particles2d::Emitter> {
    static constexpr const char* name = "haylen.ParticleEmitter";
    using Storage = std::shared_ptr<particles2d::Emitter>;
};

template <> struct Type<particles2d::Effect> {
    static constexpr const char* name = "haylen.ParticleEffect";
    using Storage = std::shared_ptr<particles2d::Effect>;
};

template <> struct Type<particles2d::System> {
    static constexpr const char* name = "haylen.ParticleSystem";
    using Storage = std::shared_ptr<particles2d::System>;
};

template <> struct Type<particles2d::Trail> {
    static constexpr const char* name = "haylen.Trail";
    using Storage = std::shared_ptr<particles2d::Trail>;
};

template <> struct Type<particles2d::ImageShape> {
    static constexpr const char* name = "haylen.ImageShape";
    using Storage = std::shared_ptr<particles2d::ImageShape>;
};

// The name table of every enum of the emitter options, which the generic names below read.
template <typename Enum> struct ParticleEnum;

template <> struct ParticleEnum<particles2d::EmitterConfig::Shape> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kShapeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::DirectionMode> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kDirectionModeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::ColorBlend> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kColorBlendNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::TintMode> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kTintModeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::FrameMode> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kFrameModeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::ParticleOrder> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kParticleOrderNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::BoundsMode> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kBoundsModeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::Attractor::Space> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kSpaceNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::Collision::Type> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kCollisionTypeNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::Collision::Result> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kCollisionResultNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::SubEmitter::Trigger> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kTriggerNames;
};
template <> struct ParticleEnum<particles2d::EmitterConfig::Light::Fade> {
    static constexpr const auto& kNames = particles2d::EmitterConfig::kFadeNames;
};

template <typename Enum>
    requires requires { ParticleEnum<Enum>::kNames; }
struct EnumNames<Enum> {
    static std::optional<Enum> fromName(std::string_view name) {
        return particles2d::EmitterConfig::fromName(ParticleEnum<Enum>::kNames, name);
    }
    static std::string_view name(Enum value) {
        return particles2d::EmitterConfig::nameOf(ParticleEnum<Enum>::kNames, value);
    }
};

// Counts accept one integer or a `{min, max}` pair, and push as a pair.
template <> struct Converter<particles2d::EmitterConfig::CountRange> {
    static void push(lua_State* L, particles2d::EmitterConfig::CountRange value) {
        lua_createtable(L, 2, 0);
        Stack::push(L, value.min);
        lua_rawseti(L, -2, 1);
        Stack::push(L, value.max);
        lua_rawseti(L, -2, 2);
    }
    static particles2d::EmitterConfig::CountRange read(lua_State* L, int index) {
        if (lua_istable(L, index)) {
            const std::vector<std::size_t> pair = Stack::read<std::vector<std::size_t>>(L, index);
            if (pair.size() != 2) {
                luaL_argerror(L, index, "a count needs an integer or the pair {min, max}");
            }
            return {pair[0], pair[1]};
        }
        const auto count = Stack::read<std::size_t>(L, index);
        return {count, count};
    }
};

} // namespace haylen::lua

namespace haylen::particles2d {

// Applies the keys present in the table on top of the given configuration, in one pass over the table.
EmitterConfig Particles2DLua::readConfig(lua_State* L, int index, EmitterConfig config, lua::Table::FieldNames extraFields) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::readFields(L, index, kConfigFields, {extraFields}, [L, &config](std::string_view key) { readKey(L, key, config); });
    return config;
}

void Particles2DLua::readKey(lua_State* L, std::string_view key, EmitterConfig& config) {
    graphics2d::DrawOrder& order = config.order;
    if (key == "layer") {
        lua::Table::readValue(L, key, order.layer);
    } else if (key == "depth") {
        lua::Table::readValue(L, key, order.depth);
    } else if (key == "sortOffset") {
        lua::Table::readValue(L, key, order.sortOffset);
    } else if (key == "visibility") {
        lua::Table::readValue(L, key, order.visibility);
    } else if (key == "blend") {
        lua::Table::readValue(L, key, order.blend);
    } else if (key == "material") {
        lua::Table::readValue(L, key, order.material);
    } else if (key == "emission") {
        lua::Table::readValue(L, key, order.emission);
    } else if (key == "unshaded") {
        lua::Table::readValue(L, key, order.unshaded);
    } else if (key == "lightMask") {
        lua::Table::readValue(L, key, order.lightMask);
    } else if (key == "subEmitters") {
        readSubEmitters(L, config.subEmitters);
    } else if (key == "light") {
        readLight(L, config);
    } else if (key == "particleLights") {
        readParticleLights(L, config);
    } else if (key == "trail") {
        readTrail(L, config.trail);
    } else {
        readSimulation(L, key, config);
    }
}

void Particles2DLua::readSimulation(lua_State* L, std::string_view key, EmitterConfig& config) {
    if (key == "rate") {
        lua::Table::readValue(L, key, config.rate);
    } else if (key == "rateOverDistance") {
        lua::Table::readValue(L, key, config.rateOverDistance);
    } else if (key == "bursts") {
        readBursts(L, config.bursts);
    } else if (key == "delay") {
        lua::Table::readValue(L, key, config.delay);
    } else if (key == "duration") {
        lua::Table::readValue(L, key, config.duration);
    } else if (key == "loop") {
        lua::Table::readValue(L, key, config.loop);
    } else if (key == "prewarm") {
        lua::Table::readValue(L, key, config.prewarm);
    } else if (key == "maxParticles") {
        lua::Table::readValue(L, key, config.maxParticles);
    } else if (key == "lifetime") {
        lua::Table::readValue(L, key, config.lifetime);
    } else if (key == "speed") {
        lua::Table::readValue(L, key, config.speed);
    } else if (key == "speedCurve") {
        readCurve(L, key, config.speedCurve);
    } else if (key == "direction") {
        lua::Table::readValue(L, key, config.direction);
    } else if (key == "directionMode") {
        lua::Table::readValue(L, key, config.directionMode);
    } else if (key == "spread") {
        lua::Table::readValue(L, key, config.spread);
    } else if (key == "inheritVelocity") {
        lua::Table::readValue(L, key, config.inheritVelocity);
    } else if (key == "gravity") {
        lua::Table::readValue(L, key, config.gravity);
    } else if (key == "radialAcceleration") {
        lua::Table::readValue(L, key, config.radialAcceleration);
    } else if (key == "tangentialAcceleration") {
        lua::Table::readValue(L, key, config.tangentialAcceleration);
    } else if (key == "damping") {
        lua::Table::readValue(L, key, config.damping);
    } else if (key == "turbulence") {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int table = lua_gettop(L);
        lua::Table::checkFields(L, table, {kTurbulenceFields});
        lua::Table::readField(L, table, "strength", config.turbulence.strength);
        lua::Table::readField(L, table, "frequency", config.turbulence.frequency);
        lua::Table::readField(L, table, "speed", config.turbulence.speed);
    } else if (key == "attractors") {
        readAttractors(L, config.attractors);
    } else if (key == "collision") {
        readCollision(L, config.collision);
    } else if (key == "bounds") {
        // The value `false` removes the bounds.
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0) {
            config.bounds.reset();
        } else {
            config.bounds = lua::Stack::read<math::Rect>(L, -1);
        }
    } else if (key == "boundsMode") {
        lua::Table::readValue(L, key, config.boundsMode);
    } else if (key == "localSpace") {
        lua::Table::readValue(L, key, config.localSpace);
    } else if (key == "pixelSnap") {
        lua::Table::readValue(L, key, config.pixelSnap);
    } else if (key == "particleOrder") {
        lua::Table::readValue(L, key, config.particleOrder);
    } else {
        readLook(L, key, config);
    }
}

void Particles2DLua::readLook(lua_State* L, std::string_view key, EmitterConfig& config) {
    if (key == "texture") {
        lua::Table::readValue(L, key, config.texture);
    } else if (key == "frames") {
        lua::Table::readValue(L, key, config.frames);
    } else if (key == "frameGrid") {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int table = lua_gettop(L);
        lua::Table::checkFields(L, table, {kGridFields});
        config.frameGrid = {.columns = 1, .rows = 1, .count = 0};
        lua::Table::readField(L, table, "columns", config.frameGrid.columns);
        lua::Table::readField(L, table, "rows", config.frameGrid.rows);
        lua::Table::readField(L, table, "count", config.frameGrid.count);
    } else if (key == "frameMode") {
        lua::Table::readValue(L, key, config.frameMode);
    } else if (key == "frameRate") {
        lua::Table::readValue(L, key, config.frameRate);
    } else if (key == "startSize") {
        lua::Table::readValue(L, key, config.startSize);
    } else if (key == "endSize") {
        lua::Table::readValue(L, key, config.endSize);
    } else if (key == "endSizeScale") {
        if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0) {
            config.endSizeScale.reset();
        } else {
            config.endSizeScale = lua::Stack::read<math::FloatRange>(L, -1);
        }
    } else if (key == "sizeCurve") {
        readCurve(L, key, config.sizeCurve);
    } else if (key == "aspect") {
        lua::Table::readValue(L, key, config.aspect);
    } else if (key == "stretch") {
        lua::Table::readValue(L, key, config.stretch);
    } else if (key == "rotation") {
        lua::Table::readValue(L, key, config.rotation);
    } else if (key == "rotationStep") {
        lua::Table::readValue(L, key, config.rotationStep);
    } else if (key == "alignToVelocity") {
        lua::Table::readValue(L, key, config.alignToVelocity);
    } else if (key == "spin") {
        lua::Table::readValue(L, key, config.spin);
    } else if (key == "spinCurve") {
        readCurve(L, key, config.spinCurve);
    } else if (key == "colors") {
        lua::Table::readValue(L, key, config.colors);
    } else if (key == "colorTimes") {
        lua::Table::readValue(L, key, config.colorTimes);
    } else if (key == "colorBlend") {
        lua::Table::readValue(L, key, config.colorBlend);
    } else if (key == "tints") {
        lua::Table::readValue(L, key, config.tints);
    } else if (key == "tintMode") {
        lua::Table::readValue(L, key, config.tintMode);
    } else if (key == "shape") {
        lua::Table::readValue(L, key, config.shape);
    } else if (key == "shapeSize") {
        lua::Table::readValue(L, key, config.shapeSize);
    } else if (key == "shapeAngle") {
        lua::Table::readValue(L, key, config.shapeAngle);
    } else if (key == "shapeArc") {
        lua::Table::readValue(L, key, config.shapeArc);
    } else if (key == "shapePoints") {
        lua::Table::readValue(L, key, config.shapePoints);
    } else if (key == "shapeThickness") {
        lua::Table::readValue(L, key, config.shapeThickness);
    } else if (key == "shapeImage") {
        lua::Table::readValue(L, key, config.shapeImage);
    } else if (key == "colorFromImage") {
        lua::Table::readValue(L, key, config.colorFromImage);
    }
}

// Curves run for every particle, on worker threads too, so they take the named and described forms and never a Lua function.
void Particles2DLua::readCurve(lua_State* L, std::string_view key, math::EasingCurve& curve) {
    lua::Table::readValue(L, key, curve);
    if (curve.getKind() == math::EasingCurve::Kind::Custom) {
        luaL_error(L, "The particle curve \"%s\" takes a curve name or a table, not a function, because it runs for every particle.", std::string(key).c_str());
    }
}

// Bursts are a list of `{time = seconds, count = particles or {min, max}, cycles, interval, probability}`.
void Particles2DLua::readBursts(lua_State* L, std::vector<EmitterConfig::Burst>& bursts) {
    luaL_checktype(L, -1, LUA_TTABLE);
    const int list = lua_gettop(L);
    bursts.clear();
    for (lua_Integer entry = 1; entry <= static_cast<lua_Integer>(lua_rawlen(L, list)); ++entry) {
        lua_rawgeti(L, list, entry);
        luaL_checktype(L, -1, LUA_TTABLE);
        const int burst = lua_gettop(L);
        lua::Table::checkFields(L, burst, {kBurstFields});
        EmitterConfig::Burst parsed;
        lua::Table::readField(L, burst, "time", parsed.time);
        lua::Table::readField(L, burst, "count", parsed.count);
        lua::Table::readField(L, burst, "cycles", parsed.cycles);
        lua::Table::readField(L, burst, "interval", parsed.interval);
        lua::Table::readField(L, burst, "probability", parsed.probability);
        bursts.push_back(parsed);
        lua_pop(L, 1);
    }
}

void Particles2DLua::readAttractors(lua_State* L, std::vector<EmitterConfig::Attractor>& attractors) {
    luaL_checktype(L, -1, LUA_TTABLE);
    const int list = lua_gettop(L);
    attractors.clear();
    for (lua_Integer entry = 1; entry <= static_cast<lua_Integer>(lua_rawlen(L, list)); ++entry) {
        lua_rawgeti(L, list, entry);
        luaL_checktype(L, -1, LUA_TTABLE);
        const int table = lua_gettop(L);
        lua::Table::checkFields(L, table, {kAttractorFields});
        EmitterConfig::Attractor attractor;
        lua::Table::readField(L, table, "x", attractor.position.x);
        lua::Table::readField(L, table, "y", attractor.position.y);
        lua::Table::readField(L, table, "strength", attractor.strength);
        lua::Table::readField(L, table, "radius", attractor.radius);
        lua::Table::readField(L, table, "killRadius", attractor.killRadius);
        lua::Table::readField(L, table, "space", attractor.space);
        attractors.push_back(attractor);
        lua_pop(L, 1);
    }
}

void Particles2DLua::readCollision(lua_State* L, EmitterConfig::Collision& collision) {
    luaL_checktype(L, -1, LUA_TTABLE);
    const int table = lua_gettop(L);
    lua::Table::checkFields(L, table, {kCollisionFields});
    lua::Table::readField(L, table, "type", collision.type);
    lua::Table::readField(L, table, "y", collision.y);
    lua::Table::readField(L, table, "area", collision.area);
    lua::Table::readField(L, table, "bounce", collision.bounce);
    lua::Table::readField(L, table, "friction", collision.friction);
    lua::Table::readField(L, table, "result", collision.result);
    lua::Table::readField(L, table, "lifeLoss", collision.lifeLoss);
}

// Each sub-emitter takes a `ParticleEffect` or an options table as its effect, with overrides on top of an effect.
void Particles2DLua::readSubEmitters(lua_State* L, std::vector<EmitterConfig::SubEmitter>& subEmitters) {
    luaL_checktype(L, -1, LUA_TTABLE);
    const int list = lua_gettop(L);
    subEmitters.clear();
    for (lua_Integer entry = 1; entry <= static_cast<lua_Integer>(lua_rawlen(L, list)); ++entry) {
        lua_rawgeti(L, list, entry);
        luaL_checktype(L, -1, LUA_TTABLE);
        const int table = lua_gettop(L);
        lua::Table::checkFields(L, table, {kSubEmitterFields});
        EmitterConfig::SubEmitter sub;
        lua_getfield(L, table, "effect");
        if (const auto* effect = lua::Userdata::test<Effect>(L, -1)) {
            if ((*effect).isComposite()) {
                luaL_error(L, "A sub-emitter needs an effect of one emitter, not a composite effect.");
            }
            sub.config = std::make_shared<EmitterConfig>((*effect).config);
        } else if (lua_istable(L, -1)) {
            sub.config = std::make_shared<EmitterConfig>(readConfig(L, lua_gettop(L), {}, {}));
        } else {
            luaL_error(L, "A sub-emitter needs a \"ParticleEffect\" or an options table in \"effect\".");
        }
        lua_pop(L, 1);
        lua_getfield(L, table, "overrides");
        if (!lua_isnil(L, -1)) {
            *sub.config = readConfig(L, lua_gettop(L), *sub.config, {});
        }
        lua_pop(L, 1);
        lua::Table::readField(L, table, "trigger", sub.trigger);
        lua::Table::readField(L, table, "count", sub.count);
        lua::Table::readField(L, table, "rate", sub.rate);
        lua::Table::readField(L, table, "probability", sub.probability);
        lua::Table::readField(L, table, "inheritVelocity", sub.inheritVelocity);
        lua::Table::readField(L, table, "inheritColor", sub.inheritColor);
        subEmitters.push_back(std::move(sub));
        lua_pop(L, 1);
    }
}

// The value `false` removes the light.
void Particles2DLua::readLight(lua_State* L, EmitterConfig& config) {
    if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0) {
        config.light.reset();
        return;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    const int table = lua_gettop(L);
    lua::Table::checkFields(L, table, {kLightFields});
    EmitterConfig::Light light;
    lua::Table::readField(L, table, "type", light.type);
    lua::Table::readField(L, table, "offset", light.offset);
    lua::Table::readField(L, table, "radius", light.radius);
    lua::Table::readField(L, table, "color", light.color);
    lua::Table::readField(L, table, "intensity", light.intensity);
    lua::Table::readField(L, table, "fade", light.fade);
    lua_getfield(L, table, "flicker");
    if (!lua_isnil(L, -1)) {
        luaL_checktype(L, -1, LUA_TTABLE);
        const int flicker = lua_gettop(L);
        lua::Table::checkFields(L, flicker, {kFlickerFields});
        lua::Table::readField(L, flicker, "speed", light.flicker.speed);
        lua::Table::readField(L, flicker, "amount", light.flicker.amount);
    }
    lua_pop(L, 1);
    config.light = light;
}

void Particles2DLua::readParticleLights(lua_State* L, EmitterConfig& config) {
    if (lua_isboolean(L, -1) && lua_toboolean(L, -1) == 0) {
        config.particleLights.reset();
        return;
    }
    luaL_checktype(L, -1, LUA_TTABLE);
    const int table = lua_gettop(L);
    lua::Table::checkFields(L, table, {kParticleLightFields});
    EmitterConfig::ParticleLights lights;
    lua::Table::readField(L, table, "radius", lights.radius);
    lua::Table::readField(L, table, "intensity", lights.intensity);
    lua::Table::readField(L, table, "max", lights.max);
    config.particleLights = lights;
}

void Particles2DLua::readTrail(lua_State* L, EmitterConfig::ParticleTrail& trail) {
    luaL_checktype(L, -1, LUA_TTABLE);
    const int table = lua_gettop(L);
    lua::Table::checkFields(L, table, {kTrailFields});
    lua::Table::readField(L, table, "length", trail.length);
    lua::Table::readField(L, table, "lifetime", trail.lifetime);
    lua::Table::readField(L, table, "widthStart", trail.widthStart);
    lua::Table::readField(L, table, "widthEnd", trail.widthEnd);
    lua::Table::readField(L, table, "colors", trail.colors);
    lua::Table::readField(L, table, "texture", trail.texture);
}

template <typename T> void Particles2DLua::setValue(lua_State* L, const char* name, const T& value) {
    lua::Stack::push(L, value);
    lua_setfield(L, -2, name);
}

// Pushes a configuration as the options table that `newEmitter` and `configure` accept, with every range as a `{min, max}` pair.
void Particles2DLua::pushConfig(lua_State* L, const EmitterConfig& config) {
    lua_createtable(L, 0, 72);
    setValue(L, "texture", config.texture);
    setValue(L, "frames", config.frames);
    if (config.frameGrid.columns > 0) {
        lua_createtable(L, 0, 3);
        setValue(L, "columns", config.frameGrid.columns);
        setValue(L, "rows", config.frameGrid.rows);
        setValue(L, "count", config.frameGrid.count);
        lua_setfield(L, -2, "frameGrid");
    }
    setValue(L, "frameMode", config.frameMode);
    setValue(L, "frameRate", config.frameRate);
    pushMotion(L, config);
    pushLook(L, config);

    const graphics2d::DrawOrder& order = config.order;
    setValue(L, "layer", order.layer);
    setValue(L, "depth", order.depth);
    setValue(L, "sortOffset", order.sortOffset);
    setValue(L, "visibility", order.visibility);
    setValue(L, "blend", order.blend);
    if (order.material.isValid()) {
        setValue(L, "material", order.material);
    }
    setValue(L, "emission", order.emission);
    setValue(L, "unshaded", order.unshaded);
    setValue(L, "lightMask", order.lightMask);
}

void Particles2DLua::pushMotion(lua_State* L, const EmitterConfig& config) {
    setValue(L, "rate", config.rate);
    setValue(L, "rateOverDistance", config.rateOverDistance);
    lua_createtable(L, static_cast<int>(config.bursts.size()), 0);
    for (std::size_t index = 0; index < config.bursts.size(); ++index) {
        const EmitterConfig::Burst& burst = config.bursts[index];
        lua_createtable(L, 0, 5);
        setValue(L, "time", burst.time);
        setValue(L, "count", burst.count);
        setValue(L, "cycles", burst.cycles);
        setValue(L, "interval", burst.interval);
        setValue(L, "probability", burst.probability);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "bursts");
    setValue(L, "delay", config.delay);
    setValue(L, "duration", config.duration);
    setValue(L, "loop", config.loop);
    setValue(L, "prewarm", config.prewarm);
    setValue(L, "maxParticles", config.maxParticles);
    setValue(L, "lifetime", config.lifetime);
    setValue(L, "speed", config.speed);
    setValue(L, "speedCurve", config.speedCurve);
    setValue(L, "direction", config.direction);
    setValue(L, "directionMode", config.directionMode);
    setValue(L, "spread", config.spread);
    setValue(L, "inheritVelocity", config.inheritVelocity);
    setValue(L, "gravity", config.gravity);
    setValue(L, "radialAcceleration", config.radialAcceleration);
    setValue(L, "tangentialAcceleration", config.tangentialAcceleration);
    setValue(L, "damping", config.damping);

    lua_createtable(L, 0, 3);
    setValue(L, "strength", config.turbulence.strength);
    setValue(L, "frequency", config.turbulence.frequency);
    setValue(L, "speed", config.turbulence.speed);
    lua_setfield(L, -2, "turbulence");
    lua_createtable(L, static_cast<int>(config.attractors.size()), 0);
    for (std::size_t index = 0; index < config.attractors.size(); ++index) {
        const EmitterConfig::Attractor& attractor = config.attractors[index];
        lua_createtable(L, 0, 6);
        setValue(L, "x", attractor.position.x);
        setValue(L, "y", attractor.position.y);
        setValue(L, "strength", attractor.strength);
        setValue(L, "radius", attractor.radius);
        setValue(L, "killRadius", attractor.killRadius);
        setValue(L, "space", attractor.space);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "attractors");
    const EmitterConfig::Collision& collision = config.collision;
    lua_createtable(L, 0, 7);
    setValue(L, "type", collision.type);
    setValue(L, "y", collision.y);
    setValue(L, "area", collision.area);
    setValue(L, "bounce", collision.bounce);
    setValue(L, "friction", collision.friction);
    setValue(L, "result", collision.result);
    setValue(L, "lifeLoss", collision.lifeLoss);
    lua_setfield(L, -2, "collision");
    if (config.bounds) {
        setValue(L, "bounds", *config.bounds);
    }
    setValue(L, "boundsMode", config.boundsMode);
    setValue(L, "localSpace", config.localSpace);
    setValue(L, "pixelSnap", config.pixelSnap);
    setValue(L, "particleOrder", config.particleOrder);
}

void Particles2DLua::pushLook(lua_State* L, const EmitterConfig& config) {
    setValue(L, "startSize", config.startSize);
    setValue(L, "endSize", config.endSize);
    if (config.endSizeScale) {
        setValue(L, "endSizeScale", *config.endSizeScale);
    }
    setValue(L, "sizeCurve", config.sizeCurve);
    setValue(L, "aspect", config.aspect);
    setValue(L, "stretch", config.stretch);
    setValue(L, "rotation", config.rotation);
    setValue(L, "rotationStep", config.rotationStep);
    setValue(L, "alignToVelocity", config.alignToVelocity);
    setValue(L, "spin", config.spin);
    setValue(L, "spinCurve", config.spinCurve);
    setValue(L, "colors", config.colors);
    setValue(L, "colorTimes", config.colorTimes);
    setValue(L, "colorBlend", config.colorBlend);
    setValue(L, "tints", config.tints);
    setValue(L, "tintMode", config.tintMode);
    setValue(L, "shape", config.shape);
    setValue(L, "shapeSize", config.shapeSize);
    setValue(L, "shapeAngle", config.shapeAngle);
    setValue(L, "shapeArc", config.shapeArc);
    setValue(L, "shapePoints", config.shapePoints);
    setValue(L, "shapeThickness", config.shapeThickness);
    setValue(L, "shapeImage", config.shapeImage);
    setValue(L, "colorFromImage", config.colorFromImage);

    lua_createtable(L, static_cast<int>(config.subEmitters.size()), 0);
    for (std::size_t index = 0; index < config.subEmitters.size(); ++index) {
        const EmitterConfig::SubEmitter& sub = config.subEmitters[index];
        lua_createtable(L, 0, 7);
        pushConfig(L, *sub.config);
        lua_setfield(L, -2, "effect");
        setValue(L, "trigger", sub.trigger);
        setValue(L, "count", sub.count);
        setValue(L, "rate", sub.rate);
        setValue(L, "probability", sub.probability);
        setValue(L, "inheritVelocity", sub.inheritVelocity);
        setValue(L, "inheritColor", sub.inheritColor);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "subEmitters");

    const EmitterConfig::ParticleTrail& trail = config.trail;
    lua_createtable(L, 0, 6);
    setValue(L, "length", trail.length);
    setValue(L, "lifetime", trail.lifetime);
    setValue(L, "widthStart", trail.widthStart);
    setValue(L, "widthEnd", trail.widthEnd);
    setValue(L, "colors", trail.colors);
    if (trail.texture.isValid()) {
        setValue(L, "texture", trail.texture);
    }
    lua_setfield(L, -2, "trail");
    if (config.light) {
        const EmitterConfig::Light& light = *config.light;
        lua_createtable(L, 0, 7);
        setValue(L, "type", light.type);
        setValue(L, "offset", light.offset);
        setValue(L, "radius", light.radius);
        setValue(L, "color", light.color);
        setValue(L, "intensity", light.intensity);
        lua_createtable(L, 0, 2);
        setValue(L, "speed", light.flicker.speed);
        setValue(L, "amount", light.flicker.amount);
        lua_setfield(L, -2, "flicker");
        setValue(L, "fade", light.fade);
        lua_setfield(L, -2, "light");
    }
    if (config.particleLights) {
        lua_createtable(L, 0, 3);
        setValue(L, "radius", config.particleLights->radius);
        setValue(L, "intensity", config.particleLights->intensity);
        setValue(L, "max", config.particleLights->max);
        lua_setfield(L, -2, "particleLights");
    }
}

// Creates an emitter from an options table with `newEmitter({texture, rate, lifetime, speed, ...})`, or from a loaded effect with `newEmitter(effect, overrides)`.
int Particles2DLua::newEmitter(lua_State* L) {
    const Effect* effect = lua::Userdata::test<Effect>(L, 1);
    if (effect != nullptr && effect->isComposite()) {
        return luaL_error(L, "The particle effect holds several emitters, so create it with \"particles2d.newSystem\".");
    }
    const int options = effect != nullptr ? 2 : 1;
    EmitterConfig config = effect != nullptr ? effect->config : EmitterConfig{};
    lua_Integer seed = 0;
    if (options == 1 || !lua_isnoneornil(L, options)) {
        config = readConfig(L, options, std::move(config), kSeedFields);
        lua::Table::readField(L, options, "seed", seed);
    }
    lua::Userdata::emplace<Emitter>(L, std::make_shared<Emitter>(std::move(config), static_cast<std::uint64_t>(seed)));
    return 1;
}

// Creates a system from a loaded effect, composite or not, with `newSystem(effect, {seed = n})`.
int Particles2DLua::newSystem(lua_State* L) {
    const Effect& effect = lua::Userdata::check<Effect>(L, 1);
    lua_Integer seed = 0;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kSeedFields});
        lua::Table::readField(L, 2, "seed", seed);
    }
    lua::Userdata::emplace<System>(L, std::make_shared<System>(effect, static_cast<std::uint64_t>(seed)));
    return 1;
}

// Reads the trail options and the draw order keys on top of the given options, so `configure` keeps whatever the table leaves out.
Trail::Options Particles2DLua::readTrailOptions(lua_State* L, int index, Trail::Options options) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kTrailOptionFields, kTrailOrderFields});
    graphics2d::DrawOrder& order = options.order;
    lua::Table::readField(L, index, "layer", order.layer);
    lua::Table::readField(L, index, "depth", order.depth);
    lua::Table::readField(L, index, "sortOffset", order.sortOffset);
    lua::Table::readField(L, index, "visibility", order.visibility);
    lua::Table::readField(L, index, "blend", order.blend);
    lua::Table::readField(L, index, "material", order.material);
    lua::Table::readField(L, index, "emission", order.emission);
    lua::Table::readField(L, index, "unshaded", order.unshaded);
    lua::Table::readField(L, index, "lightMask", order.lightMask);
    lua::Table::readField(L, index, "lifetime", options.lifetime);
    lua::Table::readField(L, index, "minDistance", options.minDistance);
    lua::Table::readField(L, index, "maxPoints", options.maxPoints);
    lua::Table::readField(L, index, "widthStart", options.widthStart);
    lua::Table::readField(L, index, "widthEnd", options.widthEnd);
    lua::Table::readField(L, index, "colors", options.colors);
    lua::Table::readField(L, index, "texture", options.texture);
    return options;
}

int Particles2DLua::newTrail(lua_State* L) {
    lua::Userdata::emplace<Trail>(L, std::make_shared<Trail>(readTrailOptions(L, 1, {})));
    return 1;
}

int Particles2DLua::emitterConfigure(lua_State* L) {
    Emitter& emitter = lua::Userdata::check<Emitter>(L, 1);
    emitter.setConfig(readConfig(L, 2, emitter.getConfig(), {}));
    return 0;
}

int Particles2DLua::emitterConfig(lua_State* L) {
    pushConfig(L, lua::Userdata::check<Emitter>(L, 1).getConfig());
    return 1;
}

int Particles2DLua::emitterPositions(lua_State* L) {
    const std::span<const math::Vec2> positions = lua::Userdata::check<Emitter>(L, 1).getPositions();
    lua::Stack::push(L, std::vector<math::Vec2>(positions.begin(), positions.end()));
    return 1;
}

// Copies `x` and `y` of every live particle into a float buffer with `readPositions(buffer[, first])` and returns how many particles fit.
int Particles2DLua::emitterReadPositions(lua_State* L) {
    const std::span<const math::Vec2> positions = lua::Userdata::check<Emitter>(L, 1).getPositions();
    const std::span<float> values = lua::Userdata::check<core::FloatBuffer>(L, 2).getValues();
    const lua_Integer first = luaL_optinteger(L, 3, 1);
    luaL_argcheck(L, first >= 1 && static_cast<std::size_t>(first) <= values.size() + 1, 3, "the first position is outside the buffer");
    const std::span<float> target = values.subspan(static_cast<std::size_t>(first - 1));
    const std::size_t count = std::min(positions.size(), target.size() / 2);
    for (std::size_t index = 0; index < count; ++index) {
        target[index * 2] = positions[index].x;
        target[index * 2 + 1] = positions[index].y;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(count));
    return 1;
}

int Particles2DLua::emitterUpdate(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).update(lua::Stack::read<float>(L, 2), lua::Runtime::getEngine(L).getJobs());
    return 0;
}

int Particles2DLua::emitterRestart(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).restart();
    return 0;
}

int Particles2DLua::emitterCycleTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Emitter>(L, 1).getCycleTime());
    return 1;
}

int Particles2DLua::emitterDraw(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).draw(lua::Runtime::getEngine(L).getRenderer2D());
    return 0;
}

int Particles2DLua::emitterBurst(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).burst(lua::Stack::read<std::size_t>(L, 2));
    return 0;
}

int Particles2DLua::emitterClear(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).clear();
    return 0;
}

int Particles2DLua::emitterCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Emitter>(L, 1).getCount());
    return 1;
}

int Particles2DLua::emitterAlive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Emitter>(L, 1).isAlive());
    return 1;
}

// Describes the live particle at an index from 1 to the count as `{x, y, vx, vy, age, lifetime, width, height, rotation, color, source}`.
int Particles2DLua::emitterParticle(lua_State* L) {
    const Emitter& emitter = lua::Userdata::check<Emitter>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, index >= 1 && static_cast<std::size_t>(index) <= emitter.getCount(), 2, "the emitter has no live particle at this index");
    const Emitter::Particle particle = emitter.getParticle(static_cast<std::size_t>(index - 1));
    lua_createtable(L, 0, 11);
    setValue(L, "x", particle.position.x);
    setValue(L, "y", particle.position.y);
    setValue(L, "vx", particle.velocity.x);
    setValue(L, "vy", particle.velocity.y);
    setValue(L, "age", particle.age);
    setValue(L, "lifetime", particle.lifetime);
    setValue(L, "width", particle.sprite.size.x);
    setValue(L, "height", particle.sprite.size.y);
    setValue(L, "rotation", particle.sprite.rotation);
    setValue(L, "color", particle.sprite.color);
    setValue(L, "source", particle.sprite.source);
    return 1;
}

// Attractors count from 1 like the list of the options.
int Particles2DLua::emitterSetAttractor(lua_State* L) {
    Emitter& emitter = lua::Userdata::check<Emitter>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    luaL_argcheck(L, index >= 1 && static_cast<std::size_t>(index) <= emitter.getConfig().attractors.size(), 2, "the emitter has no attractor at this index");
    emitter.setAttractor(static_cast<std::size_t>(index - 1), {lua::Stack::read<float>(L, 3), lua::Stack::read<float>(L, 4)});
    return 0;
}

int Particles2DLua::emitterSetShapePoints(lua_State* L) {
    lua::Userdata::check<Emitter>(L, 1).setShapePoints(lua::Stack::read<std::vector<math::Vec2>>(L, 2));
    return 0;
}

// The emitter keeps the world in its user value, so the world lives as long as the emitter collides with it.
int Particles2DLua::emitterSetCollisionWorld(lua_State* L) {
    Emitter& emitter = lua::Userdata::check<Emitter>(L, 1);
    if (lua_isnoneornil(L, 2)) {
        emitter.setCollisionWorld(nullptr);
        lua_pushnil(L);
        lua::Userdata::setField(L, 1, "collisionWorld", lua_gettop(L));
        return 0;
    }
    const physics2d::World& world = lua::Userdata::check<physics2d::World>(L, 2);
    physics2d::CollisionFilter filter;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kFilterFields});
        filter = physics2d::ShapeLua::readFilter(L, 3, {});
    }
    emitter.setCollisionWorld(&world, filter);
    lua::Userdata::setField(L, 1, "collisionWorld", 2);
    return 0;
}

int Particles2DLua::effectTexturePath(lua_State* L) {
    const Effect& effect = lua::Userdata::check<Effect>(L, 1);
    if (effect.isComposite()) {
        lua_pushnil(L);
    } else {
        lua::Stack::push(L, effect.files.texture);
    }
    return 1;
}

int Particles2DLua::effectConfig(lua_State* L) {
    const Effect& effect = lua::Userdata::check<Effect>(L, 1);
    if (effect.isComposite()) {
        lua_pushnil(L);
    } else {
        pushConfig(L, effect.config);
    }
    return 1;
}

int Particles2DLua::effectComposite(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Effect>(L, 1).isComposite());
    return 1;
}

// Lists the emitters of a composite effect as `{name, offset, scale, texturePath, config}`.
int Particles2DLua::effectParts(lua_State* L) {
    const Effect& effect = lua::Userdata::check<Effect>(L, 1);
    lua_createtable(L, static_cast<int>(effect.parts.size()), 0);
    for (std::size_t index = 0; index < effect.parts.size(); ++index) {
        const Effect::Part& part = effect.parts[index];
        lua_createtable(L, 0, 5);
        setValue(L, "name", part.name);
        setValue(L, "offset", part.offset);
        setValue(L, "scale", part.scale);
        setValue(L, "texturePath", part.files.texture);
        pushConfig(L, part.config);
        lua_setfield(L, -2, "config");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int Particles2DLua::systemUpdate(lua_State* L) {
    lua::Userdata::check<System>(L, 1).update(lua::Stack::read<float>(L, 2), lua::Runtime::getEngine(L).getJobs());
    return 0;
}

int Particles2DLua::systemDraw(lua_State* L) {
    lua::Userdata::check<System>(L, 1).draw(lua::Runtime::getEngine(L).getRenderer2D());
    return 0;
}

int Particles2DLua::systemRestart(lua_State* L) {
    lua::Userdata::check<System>(L, 1).restart();
    return 0;
}

int Particles2DLua::systemClear(lua_State* L) {
    lua::Userdata::check<System>(L, 1).clear();
    return 0;
}

int Particles2DLua::systemEmitter(lua_State* L) {
    const std::shared_ptr<Emitter> emitter = lua::Userdata::check<System>(L, 1).findEmitter(lua::Stack::read<std::string_view>(L, 2));
    if (!emitter) {
        lua_pushnil(L);
        return 1;
    }
    lua::Userdata::emplace<Emitter>(L, emitter);
    return 1;
}

int Particles2DLua::systemEmitters(lua_State* L) {
    const System& system = lua::Userdata::check<System>(L, 1);
    lua_createtable(L, static_cast<int>(system.getEmitterCount()), 0);
    for (std::size_t index = 0; index < system.getEmitterCount(); ++index) {
        lua::Userdata::emplace<Emitter>(L, system.getEmitter(index));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int Particles2DLua::systemCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<System>(L, 1).getCount());
    return 1;
}

int Particles2DLua::systemAlive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<System>(L, 1).isAlive());
    return 1;
}

int Particles2DLua::systemEmitting(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<System>(L, 1).isEmitting());
    return 1;
}

int Particles2DLua::systemSetEmitting(lua_State* L) {
    lua::Userdata::check<System>(L, 1).setEmitting(lua::Stack::read<bool>(L, 3));
    return 0;
}

int Particles2DLua::trailUpdate(lua_State* L) {
    lua::Userdata::check<Trail>(L, 1).update(lua::Stack::read<float>(L, 2));
    return 0;
}

int Particles2DLua::trailDraw(lua_State* L) {
    lua::Userdata::check<Trail>(L, 1).draw(lua::Runtime::getEngine(L).getRenderer2D());
    return 0;
}

int Particles2DLua::trailClear(lua_State* L) {
    lua::Userdata::check<Trail>(L, 1).clear();
    return 0;
}

int Particles2DLua::trailConfigure(lua_State* L) {
    Trail& trail = lua::Userdata::check<Trail>(L, 1);
    trail.setOptions(readTrailOptions(L, 2, trail.getOptions()));
    return 0;
}

void Particles2DLua::pushTrailOptions(lua_State* L, const Trail::Options& options) {
    lua_createtable(L, 0, 10);
    setValue(L, "lifetime", options.lifetime);
    setValue(L, "minDistance", options.minDistance);
    setValue(L, "maxPoints", options.maxPoints);
    setValue(L, "widthStart", options.widthStart);
    setValue(L, "widthEnd", options.widthEnd);
    setValue(L, "colors", options.colors);
    if (options.texture.isValid()) {
        setValue(L, "texture", options.texture);
    }
    setValue(L, "layer", options.order.layer);
    setValue(L, "depth", options.order.depth);
    setValue(L, "sortOffset", options.order.sortOffset);
    setValue(L, "visibility", options.order.visibility);
    setValue(L, "blend", options.order.blend);
    if (options.order.material.isValid()) {
        setValue(L, "material", options.order.material);
    }
    setValue(L, "emission", options.order.emission);
    setValue(L, "unshaded", options.order.unshaded);
    setValue(L, "lightMask", options.order.lightMask);
}

int Particles2DLua::trailConfig(lua_State* L) {
    pushTrailOptions(L, lua::Userdata::check<Trail>(L, 1).getOptions());
    return 1;
}

int Particles2DLua::trailCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Trail>(L, 1).getCount());
    return 1;
}

int Particles2DLua::trailAlive(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Trail>(L, 1).isAlive());
    return 1;
}

int Particles2DLua::shapeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ImageShape>(L, 1).getPoints().size());
    return 1;
}

int Particles2DLua::shapeWidth(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ImageShape>(L, 1).getSize().x);
    return 1;
}

int Particles2DLua::shapeHeight(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ImageShape>(L, 1).getSize().y);
    return 1;
}

int Particles2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newEmitter", &lua::Binding::native<&newEmitter>},
        {"newSystem", &lua::Binding::native<&newSystem>},
        {"newTrail", &lua::Binding::native<&newTrail>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Particles2DLua::pushEffect(lua_State* L, std::shared_ptr<Effect> effect) {
    lua::Userdata::emplace<Effect>(L, std::move(effect));
}

void Particles2DLua::pushImageShape(lua_State* L, std::shared_ptr<ImageShape> shape) {
    lua::Userdata::emplace<ImageShape>(L, std::move(shape));
}

void Particles2DLua::install(lua_State* L) {
    lua::ClassBuilder<Effect>(L).property("texturePath", &effectTexturePath).property("config", &effectConfig).property("composite", &effectComposite).property("parts", &effectParts).meta("__eq", &lua::Userdata::equal<Effect>).install();
    lua::ClassBuilder<ImageShape>(L).property("count", &shapeCount).property("width", &shapeWidth).property("height", &shapeHeight).meta("__eq", &lua::Userdata::equal<ImageShape>).install();
    lua::ClassBuilder<Emitter>(L).function("configure", &lua::Binding::native<&emitterConfigure>).function("positions", &emitterPositions).function("readPositions", &lua::Binding::native<&emitterReadPositions>).function("update", &lua::Binding::native<&emitterUpdate>).function("draw", &lua::Binding::native<&emitterDraw>).function("burst", &lua::Binding::native<&emitterBurst>).function("clear", &emitterClear).function("restart", &lua::Binding::native<&emitterRestart>).function("particle", &lua::Binding::native<&emitterParticle>).function("setAttractor", &lua::Binding::native<&emitterSetAttractor>).function("setShapePoints", &lua::Binding::native<&emitterSetShapePoints>).function("setCollisionWorld", &lua::Binding::native<&emitterSetCollisionWorld>).nestedField<&Emitter::position, &math::Vec2::x>("x").nestedField<&Emitter::position, &math::Vec2::y>("y").field<&Emitter::position>("position").field<&Emitter::scale>("scale").field<&Emitter::rotation>("rotation").field<&Emitter::emitting>("emitting").property("count", &emitterCount).property("alive", &emitterAlive).property("cycleTime", &emitterCycleTime).property("config", &emitterConfig).meta("__eq", &lua::Userdata::equal<Emitter>).install();
    lua::ClassBuilder<System>(L).function("update", &lua::Binding::native<&systemUpdate>).function("draw", &lua::Binding::native<&systemDraw>).function("restart", &systemRestart).function("clear", &systemClear).function("emitter", &lua::Binding::native<&systemEmitter>).function("emitters", &systemEmitters).nestedField<&System::position, &math::Vec2::x>("x").nestedField<&System::position, &math::Vec2::y>("y").field<&System::position>("position").field<&System::scale>("scale").field<&System::rotation>("rotation").property("emitting", &systemEmitting, &lua::Binding::native<&systemSetEmitting>).property("count", &systemCount).property("alive", &systemAlive).install();
    lua::ClassBuilder<Trail>(L).function("update", &lua::Binding::native<&trailUpdate>).function("draw", &lua::Binding::native<&trailDraw>).function("clear", &trailClear).function("configure", &lua::Binding::native<&trailConfigure>).nestedField<&Trail::position, &math::Vec2::x>("x").nestedField<&Trail::position, &math::Vec2::y>("y").field<&Trail::position>("position").field<&Trail::emitting>("emitting").property("count", &trailCount).property("alive", &trailAlive).property("config", &trailConfig).install();
    lua::Binding::preload(L, "haylen.particles2d", &open);
}

} // namespace haylen::particles2d
