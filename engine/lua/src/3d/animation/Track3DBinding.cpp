#include "3d/animation/Track3DBinding.hpp"

#include <lua.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "core/FloatBufferBinding.hpp"
#include "haylen/core/FloatBuffer.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/math/Vec3.hpp"
#include "haylen/math/Vec4.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::Track3D;

const Track3D& Track3DBinding::check(lua_State* L, int index) {
    return *lua::Userdata::check<ScriptedTrack3D>(L, index).track;
}

void Track3DBinding::push(lua_State* L, std::shared_ptr<const Track3D> track) {
    lua::Userdata::emplace<ScriptedTrack3D>(L, ScriptedTrack3D{.track = std::move(track)});
}

void Track3DBinding::pushBuffer(lua_State* L, std::span<const float> values) {
    auto buffer = std::make_shared<haylen::core::FloatBuffer>(values.size());
    buffer->set(0, values);
    lua::Userdata::emplace<haylen::core::FloatBuffer>(L, std::move(buffer));
}

void Track3DBinding::pushValue(lua_State* L, const Track3D& track, std::span<const float> value) {
    switch (track.getType()) {
    case Track3D::Type::Number:
        lua::Stack::push(L, value[0]);
        return;
    case Track3D::Type::Boolean:
        lua_pushboolean(L, value[0] != 0.0F ? 1 : 0);
        return;
    case Track3D::Type::String:
        lua::Stack::push(L, track.getStrings()[static_cast<std::size_t>(value[0])]);
        return;
    case Track3D::Type::Vec2:
        lua::Stack::push(L, haylen::math::Vec2{value[0], value[1]});
        return;
    case Track3D::Type::Vec3:
        lua::Stack::push(L, haylen::math::Vec3{value[0], value[1], value[2]});
        return;
    case Track3D::Type::Vec4:
        lua::Stack::push(L, haylen::math::Vec4{value[0], value[1], value[2], value[3]});
        return;
    case Track3D::Type::Quat:
        lua::Stack::push(L, haylen::math::Quat{value[0], value[1], value[2], value[3]});
        return;
    case Track3D::Type::Color:
        lua::Stack::push(L, haylen::math::Color::fromLinear({value[0], value[1], value[2], value[3]}));
        return;
    case Track3D::Type::Weights:
        lua_createtable(L, static_cast<int>(value.size()), 0);
        for (std::size_t index = 0; index < value.size(); ++index) {
            lua::Stack::push(L, value[index]);
            lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
        }

        return;
    }
}

int Track3DBinding::create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kFields});
    Track3D::Description description;
    lua::Table::readField(L, 1, "target", description.target);
    lua::Table::readField(L, 1, "property", description.property);
    lua::Table::readField(L, 1, "type", description.type);
    lua::Table::readField(L, 1, "interpolation", description.interpolation);
    for (const auto& [key, numbers] : {std::pair{"times", &description.times}, std::pair{"values", &description.values}, std::pair{"handles", &description.handles}}) {
        lua_getfield(L, 1, key);
        if (!lua_isnil(L, -1)) {
            lua::core::FloatBufferBinding::readNumbers(L, lua_gettop(L), key, *numbers);
        }

        lua_pop(L, 1);
    }

    lua_getfield(L, 1, "strings");
    if (!lua_isnil(L, -1)) {
        lua::Table::requireType(L, "strings", LUA_TTABLE);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer place = 1; place <= count; ++place) {
            lua_rawgeti(L, -1, place);
            lua::Table::requireItemType(L, "strings", place, LUA_TSTRING);
            description.strings.emplace_back(lua::Stack::read<std::string_view>(L, -1));
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    push(L, std::make_shared<const Track3D>(std::move(description)));
    return 1;
}

int Track3DBinding::target(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getTarget());
    return 1;
}

int Track3DBinding::property(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getProperty());
    return 1;
}

int Track3DBinding::type(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getType());
    return 1;
}

int Track3DBinding::interpolation(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getInterpolation());
    return 1;
}

int Track3DBinding::storage(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getStorage());
    return 1;
}

int Track3DBinding::keyCount(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getKeyCount());
    return 1;
}

int Track3DBinding::componentCount(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getComponentCount());
    return 1;
}

int Track3DBinding::startTime(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getStartTime());
    return 1;
}

int Track3DBinding::endTime(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getEndTime());
    return 1;
}

int Track3DBinding::quantizationError(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getQuantizationError());
    return 1;
}

int Track3DBinding::memorySize(lua_State* L) {
    lua::Stack::push(L, check(L, 1).getMemorySize());
    return 1;
}

int Track3DBinding::times(lua_State* L) {
    pushBuffer(L, check(L, 1).getTimes());
    return 1;
}

int Track3DBinding::values(lua_State* L) {
    pushBuffer(L, check(L, 1).getValues());
    return 1;
}

int Track3DBinding::handles(lua_State* L) {
    pushBuffer(L, check(L, 1).getHandles());
    return 1;
}

int Track3DBinding::strings(lua_State* L) {
    const std::span<const std::string> keys = check(L, 1).getStrings();
    lua_createtable(L, static_cast<int>(keys.size()), 0);
    for (std::size_t index = 0; index < keys.size(); ++index) {
        lua::Stack::push(L, keys[index]);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Returns the value at a time with `sample(time)`, in the Lua type of the track.
int Track3DBinding::sample(lua_State* L) {
    const Track3D& track = check(L, 1);
    const float time = lua::Stack::read<float>(L, 2);
    std::vector<float> value(track.getComponentCount());
    std::size_t cursor = 0;
    track.sample(time, value, cursor);
    pushValue(L, track, value);
    return 1;
}

void Track3DBinding::install(lua_State* L) {
    lua::ClassBuilder<ScriptedTrack3D>(L).property("target", &lua::Binding::native<&target>).property("property", &lua::Binding::native<&property>).property("type", &lua::Binding::native<&type>).property("interpolation", &lua::Binding::native<&interpolation>).property("storage", &lua::Binding::native<&storage>).property("keyCount", &lua::Binding::native<&keyCount>).property("componentCount", &lua::Binding::native<&componentCount>).property("startTime", &lua::Binding::native<&startTime>).property("endTime", &lua::Binding::native<&endTime>).property("quantizationError", &lua::Binding::native<&quantizationError>).property("memorySize", &lua::Binding::native<&memorySize>).function("times", &lua::Binding::native<&times>).function("values", &lua::Binding::native<&values>).function("handles", &lua::Binding::native<&handles>).function("strings", &lua::Binding::native<&strings>).function("sample", &lua::Binding::native<&sample>).install();
}

} // namespace haylen::lua::animation3d
