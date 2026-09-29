#include "2d/particles/Particles2DLua.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include "core/FloatBufferLua.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/particles/Effect.hpp"
#include "haylen/2d/particles/Emitter.hpp"
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

template <> struct EnumNames<particles2d::EmitterConfig::Shape> {
    static std::optional<particles2d::EmitterConfig::Shape> fromName(std::string_view name) {
        return particles2d::EmitterConfig::shapeFromName(name);
    }
    static std::string_view name(particles2d::EmitterConfig::Shape value) {
        return particles2d::EmitterConfig::shapeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::particles2d {

// Ranges accept a single number or the pair {min, max}.
void Particles2DLua::readRange(lua_State* L, int table, const char* name, math::FloatRange& range) {
    lua_getfield(L, table, name);
    if (lua_type(L, -1) == LUA_TNUMBER) {
        range.min = range.max = static_cast<float>(lua_tonumber(L, -1));
    } else if (!lua_isnil(L, -1)) {
        const math::Vec2 pair = lua::Stack::read<math::Vec2>(L, -1);
        range = {pair.x, pair.y};
    }
    lua_pop(L, 1);
}

// Bursts are a list of {time = seconds, count = particles}.
void Particles2DLua::readBursts(lua_State* L, int table, std::vector<EmitterConfig::Burst>& bursts) {
    lua_getfield(L, table, "bursts");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        return;
    }
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
        bursts.push_back(parsed);
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

// Applies the fields present in the table on top of the given configuration.
EmitterConfig Particles2DLua::readConfig(lua_State* L, int index, EmitterConfig config, lua::Table::FieldNames extraFields) {
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kConfigFields, extraFields});
    lua::Table::readField(L, index, "texture", config.texture);
    lua::Table::readField(L, index, "frames", config.frames);
    lua::Table::readField(L, index, "rate", config.rate);
    readBursts(L, index, config.bursts);
    lua::Table::readField(L, index, "duration", config.duration);
    lua::Table::readField(L, index, "loop", config.loop);
    lua::Table::readField(L, index, "prewarm", config.prewarm);
    lua::Table::readField(L, index, "maxParticles", config.maxParticles);
    readRange(L, index, "lifetime", config.lifetime);
    readRange(L, index, "speed", config.speed);
    lua::Table::readField(L, index, "direction", config.direction);
    lua::Table::readField(L, index, "spread", config.spread);
    lua::Table::readField(L, index, "gravity", config.gravity);
    readRange(L, index, "radialAcceleration", config.radialAcceleration);
    readRange(L, index, "tangentialAcceleration", config.tangentialAcceleration);
    lua::Table::readField(L, index, "damping", config.damping);
    readRange(L, index, "startSize", config.startSize);
    readRange(L, index, "endSize", config.endSize);
    readRange(L, index, "spin", config.spin);
    lua::Table::readField(L, index, "colors", config.colors);
    lua::Table::readField(L, index, "shape", config.shape);
    lua::Table::readField(L, index, "shapeSize", config.shapeSize);
    lua::Table::readField(L, index, "localSpace", config.localSpace);
    lua::Table::readField(L, index, "layer", config.order.layer);
    lua::Table::readField(L, index, "depth", config.order.depth);
    lua::Table::readField(L, index, "blend", config.order.blend);
    return config;
}

void Particles2DLua::pushRange(lua_State* L, const char* name, math::FloatRange range) {
    lua_createtable(L, 2, 0);
    lua::Stack::push(L, range.min);
    lua_rawseti(L, -2, 1);
    lua::Stack::push(L, range.max);
    lua_rawseti(L, -2, 2);
    lua_setfield(L, -2, name);
}

template <typename T> void Particles2DLua::setValue(lua_State* L, const char* name, const T& value) {
    lua::Stack::push(L, value);
    lua_setfield(L, -2, name);
}

// Pushes a configuration as the options table that newEmitter and configure accept, with every range as a {min, max} pair.
void Particles2DLua::pushConfig(lua_State* L, const EmitterConfig& config) {
    lua_createtable(L, 0, 26);
    setValue(L, "texture", config.texture);
    setValue(L, "frames", config.frames);

    setValue(L, "rate", config.rate);
    lua_createtable(L, static_cast<int>(config.bursts.size()), 0);
    for (std::size_t index = 0; index < config.bursts.size(); ++index) {
        lua_createtable(L, 0, 2);
        setValue(L, "time", config.bursts[index].time);
        setValue(L, "count", config.bursts[index].count);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    lua_setfield(L, -2, "bursts");
    setValue(L, "duration", config.duration);
    setValue(L, "loop", config.loop);
    setValue(L, "prewarm", config.prewarm);
    setValue(L, "maxParticles", config.maxParticles);

    pushRange(L, "lifetime", config.lifetime);
    pushRange(L, "speed", config.speed);
    setValue(L, "direction", config.direction);
    setValue(L, "spread", config.spread);
    setValue(L, "gravity", config.gravity);
    pushRange(L, "radialAcceleration", config.radialAcceleration);
    pushRange(L, "tangentialAcceleration", config.tangentialAcceleration);
    setValue(L, "damping", config.damping);

    pushRange(L, "startSize", config.startSize);
    pushRange(L, "endSize", config.endSize);
    pushRange(L, "spin", config.spin);
    setValue(L, "colors", config.colors);
    setValue(L, "shape", config.shape);
    setValue(L, "shapeSize", config.shapeSize);
    setValue(L, "localSpace", config.localSpace);

    setValue(L, "layer", config.order.layer);
    setValue(L, "depth", config.order.depth);
    setValue(L, "blend", config.order.blend);
}

// Creates an emitter from an options table with newEmitter({texture, rate, lifetime, speed, ...}), or from a loaded effect with newEmitter(effect, overrides).
int Particles2DLua::newEmitter(lua_State* L) {
    const int options = lua::Userdata::test<Effect>(L, 1) != nullptr ? 2 : 1;
    EmitterConfig config = options == 2 ? lua::Userdata::check<Effect>(L, 1).config : EmitterConfig{};
    lua_Integer seed = 0;
    if (options == 1 || !lua_isnoneornil(L, options)) {
        config = readConfig(L, options, std::move(config), kSeedFields);
        lua::Table::readField(L, options, "seed", seed);
    }
    lua::Userdata::emplace<Emitter>(L, std::make_shared<Emitter>(std::move(config), static_cast<std::uint64_t>(seed)));
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

// Copies x and y of every live particle into a float buffer with readPositions(buffer[, first]) and returns how many particles fit.
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

int Particles2DLua::effectTexturePath(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Effect>(L, 1).texturePath);
    return 1;
}

int Particles2DLua::effectConfig(lua_State* L) {
    pushConfig(L, lua::Userdata::check<Effect>(L, 1).config);
    return 1;
}

int Particles2DLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newEmitter", &lua::Binding::native<&newEmitter>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void Particles2DLua::pushEffect(lua_State* L, std::shared_ptr<Effect> effect) {
    lua::Userdata::emplace<Effect>(L, std::move(effect));
}

void Particles2DLua::install(lua_State* L) {
    lua::ClassBuilder<Effect>(L).property("texturePath", &effectTexturePath).property("config", &effectConfig).meta("__eq", &lua::Userdata::equal<Effect>).install();
    lua::ClassBuilder<Emitter>(L).function("configure", &lua::Binding::native<&emitterConfigure>).function("positions", &emitterPositions).function("readPositions", &lua::Binding::native<&emitterReadPositions>).function("update", &lua::Binding::native<&emitterUpdate>).function("draw", &lua::Binding::native<&emitterDraw>).function("burst", &lua::Binding::native<&emitterBurst>).function("clear", &emitterClear).function("restart", &lua::Binding::native<&emitterRestart>).nestedField<&Emitter::position, &math::Vec2::x>("x").nestedField<&Emitter::position, &math::Vec2::y>("y").field<&Emitter::position>("position").field<&Emitter::emitting>("emitting").property("count", &emitterCount).property("alive", &emitterAlive).property("cycleTime", &emitterCycleTime).property("config", &emitterConfig).install();
    lua::Binding::preload(L, "haylen.particles2d", &open);
}

} // namespace haylen::particles2d
