#include "3d/animation/PoseBatch3DBinding.hpp"

#include <lua.hpp>

#include <algorithm>
#include <format>
#include <span>
#include <stdexcept>

#include "3d/animation/Clip3DBinding.hpp"
#include "3d/animation/Skin3DBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::PoseBatch3D;

PoseBatch3D& PoseBatch3DBinding::check(lua_State* L) {
    return lua::Userdata::check<PoseBatch3D>(L, 1);
}

std::size_t PoseBatch3DBinding::readCharacter(lua_State* L, int index) {
    const lua_Integer character = lua::Stack::read<lua_Integer>(L, index);
    luaL_argcheck(L, character >= 1, index, "characters are numbered from one");
    return static_cast<std::size_t>(character - 1);
}

int PoseBatch3DBinding::create(lua_State* L) {
    lua::Userdata::emplace<PoseBatch3D>(L, std::make_shared<PoseBatch3D>(Skin3DBinding::checkSkin(L, 1)));
    return 1;
}

// Adds a character with `add(clip[, {time, speed}])` and returns its place, counted from 1.
int PoseBatch3DBinding::add(lua_State* L) {
    PoseBatch3D& batch = check(L);
    const std::shared_ptr<haylen::animation3d::Clip3D>& clip = lua::Userdata::checkShared<haylen::animation3d::Clip3D>(L, 2);
    PoseBatch3D::Options options;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kFields});
        lua::Table::readField(L, 3, "time", options.time);
        lua::Table::readField(L, 3, "speed", options.speed);
    }

    lua::Stack::push(L, batch.add(clip, options) + 1);
    return 1;
}

int PoseBatch3DBinding::remove(lua_State* L) {
    check(L).remove(readCharacter(L, 2));
    return 0;
}

int PoseBatch3DBinding::setClip(lua_State* L) {
    check(L).setClip(readCharacter(L, 2), lua::Userdata::checkShared<haylen::animation3d::Clip3D>(L, 3));
    return 0;
}

int PoseBatch3DBinding::setTime(lua_State* L) {
    check(L).setTime(readCharacter(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int PoseBatch3DBinding::setSpeed(lua_State* L) {
    check(L).setSpeed(readCharacter(L, 2), lua::Stack::read<float>(L, 3));
    return 0;
}

int PoseBatch3DBinding::time(lua_State* L) {
    lua::Stack::push(L, check(L).getTime(readCharacter(L, 2)));
    return 1;
}

// Sets the time of every character with `setTimes(buffer[, first])`, one number each from the value `first` of the buffer on.
int PoseBatch3DBinding::setTimes(lua_State* L) {
    PoseBatch3D& batch = check(L);
    const std::span<float> times = lua::core::FloatBufferBinding::readRange(L, 2);
    if (times.size() < batch.getCount()) {
        throw std::invalid_argument(std::format("The times of {} characters take {} values, while the buffer holds {} from its first one.", batch.getCount(), batch.getCount(), times.size()));
    }

    batch.setTimes(times.first(batch.getCount()));
    return 0;
}

int PoseBatch3DBinding::update(lua_State* L) {
    check(L).update(lua::Stack::read<float>(L, 2), &lua::Runtime::getEngine(L).getJobs());
    return 0;
}

// Writes the palettes of every character with `readPalettes(buffer[, first])`, the twelve floats of each joint of each character one after another.
int PoseBatch3DBinding::readPalettes(lua_State* L) {
    const std::span<const float> palettes = check(L).getPalettes();
    const std::span<float> target = lua::core::FloatBufferBinding::readRange(L, 2);
    if (target.size() < palettes.size()) {
        throw std::invalid_argument(std::format("The palettes of the batch take {} values, while the buffer holds {} from its first one.", palettes.size(), target.size()));
    }

    std::ranges::copy(palettes, target.begin());
    return 0;
}

int PoseBatch3DBinding::count(lua_State* L) {
    lua::Stack::push(L, check(L).getCount());
    return 1;
}

// The floats of the palette of one character.
int PoseBatch3DBinding::paletteSize(lua_State* L) {
    lua::Stack::push(L, check(L).getSkin().getJointCount() * haylen::animation3d::Skin3D::kRowFloats);
    return 1;
}

void PoseBatch3DBinding::install(lua_State* L) {
    lua::ClassBuilder<PoseBatch3D>(L).property("count", &lua::Binding::native<&count>).property("paletteSize", &lua::Binding::native<&paletteSize>).function("add", &lua::Binding::native<&add>).function("remove", &lua::Binding::native<&remove>).function("setClip", &lua::Binding::native<&setClip>).function("setTime", &lua::Binding::native<&setTime>).function("setSpeed", &lua::Binding::native<&setSpeed>).function("time", &lua::Binding::native<&time>).function("setTimes", &lua::Binding::native<&setTimes>).function("update", &lua::Binding::native<&update>).function("readPalettes", &lua::Binding::native<&readPalettes>).install();
}

} // namespace haylen::lua::animation3d
