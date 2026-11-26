#include "3d/animation/Clip3DBinding.hpp"

#include <lua.hpp>

#include <format>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "3d/animation/Track3DBinding.hpp"
#include "3d/gltf/GltfBinding.hpp"
#include "core/FloatBufferBinding.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::animation3d {

using haylen::animation3d::Clip3D;
using haylen::animation3d::Track3D;

const std::shared_ptr<Clip3D>& Clip3DBinding::check(lua_State* L, int index) {
    return lua::Userdata::checkShared<Clip3D>(L, index);
}

// A clip never changes once made, so Lua holds the same object that players and C++ share.
void Clip3DBinding::push(lua_State* L, std::shared_ptr<const Clip3D> clip) {
    lua::Userdata::emplace<Clip3D>(L, std::const_pointer_cast<Clip3D>(std::move(clip)));
}

std::vector<Clip3D::Event> Clip3DBinding::readEvents(lua_State* L, int index) {
    std::vector<Clip3D::Event> events;
    lua::Table::requireType(L, "events", LUA_TTABLE);
    const lua_Integer count = luaL_len(L, index);
    for (lua_Integer place = 1; place <= count; ++place) {
        lua_rawgeti(L, index, place);
        lua::Table::requireItemType(L, "events", place, LUA_TTABLE);
        const int entry = lua_gettop(L);
        lua::Table::checkFields(L, entry, {kEventFields});
        Clip3D::Event event;
        lua::Table::readField(L, entry, "time", event.time);
        lua::Table::readField(L, entry, "name", event.name);
        lua_getfield(L, entry, "data");
        if (!lua_isnil(L, -1)) {
            event.data = lua::JsonConverter::read(L, lua_gettop(L));
        }

        lua_pop(L, 2);
        events.push_back(std::move(event));
    }

    return events;
}

int Clip3DBinding::create(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kFields});
    Clip3D::Description description;
    lua::Table::readField(L, 1, "name", description.name);
    lua_getfield(L, 1, "tracks");
    if (!lua_isnil(L, -1)) {
        lua::Table::requireType(L, "tracks", LUA_TTABLE);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer place = 1; place <= count; ++place) {
            lua_rawgeti(L, -1, place);
            const ScriptedTrack3D* item = lua::Userdata::test<ScriptedTrack3D>(L, -1);
            if (item == nullptr) {
                throw std::invalid_argument(std::format("The item {} of the option \"tracks\" is not a track.", place));
            }

            description.tracks.push_back(*item->track);
            lua_pop(L, 1);
        }
    }

    lua_pop(L, 1);
    lua_getfield(L, 1, "events");
    if (!lua_isnil(L, -1)) {
        description.events = readEvents(L, lua_gettop(L));
    }

    lua_pop(L, 1);
    lua_getfield(L, 1, "duration");
    if (!lua_isnil(L, -1)) {
        float duration = 0.0F;
        lua::Table::readValue(L, "duration", duration);
        description.duration = duration;
    }

    lua_pop(L, 1);
    push(L, std::make_shared<const Clip3D>(std::move(description)));
    return 1;
}

int Clip3DBinding::fromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    const std::size_t animation = lua::gltf::GltfBinding::checkObject(L, 2, "animations", document.animations.size());
    push(L, std::make_shared<const Clip3D>(Clip3D::fromGltf(document, animation)));
    return 1;
}

int Clip3DBinding::allFromGltf(lua_State* L) {
    const haylen::gltf::Document& document = lua::Userdata::check<haylen::gltf::Document>(L, 1);
    lua_createtable(L, static_cast<int>(document.animations.size()), 0);
    for (std::size_t animation = 0; animation < document.animations.size(); ++animation) {
        push(L, std::make_shared<const Clip3D>(Clip3D::fromGltf(document, animation)));
        lua_rawseti(L, -2, static_cast<lua_Integer>(animation + 1));
    }

    return 1;
}

int Clip3DBinding::fromMorphSequence(lua_State* L) {
    Clip3D::MorphSequence sequence;
    sequence.target = lua::Stack::read<std::string>(L, 1);
    sequence.targetCount = lua::Stack::read<std::size_t>(L, 2);
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kSequenceFields});
        lua::Table::readField(L, 3, "name", sequence.name);
        lua::Table::readField(L, 3, "framesPerSecond", sequence.framesPerSecond);
        lua::Table::readField(L, 3, "loop", sequence.loop);
    }

    push(L, std::make_shared<const Clip3D>(Clip3D::fromMorphSequence(sequence)));
    return 1;
}

int Clip3DBinding::name(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getName());
    return 1;
}

int Clip3DBinding::duration(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getDuration());
    return 1;
}

int Clip3DBinding::valueCount(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getValueCount());
    return 1;
}

int Clip3DBinding::memorySize(lua_State* L) {
    lua::Stack::push(L, check(L, 1)->getMemorySize());
    return 1;
}

// Lists the tracks, each of which keeps the clip alive.
int Clip3DBinding::tracks(lua_State* L) {
    const std::shared_ptr<Clip3D>& clip = check(L, 1);
    const std::span<const Track3D> list = clip->getTracks();
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (std::size_t index = 0; index < list.size(); ++index) {
        Track3DBinding::push(L, std::shared_ptr<const Track3D>(clip, &list[index]));
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

int Clip3DBinding::events(lua_State* L) {
    const std::span<const Clip3D::Event> list = check(L, 1)->getEvents();
    lua_createtable(L, static_cast<int>(list.size()), 0);
    for (std::size_t index = 0; index < list.size(); ++index) {
        lua_createtable(L, 0, 3);
        lua::Stack::push(L, list[index].time);
        lua_setfield(L, -2, "time");
        lua::Stack::push(L, list[index].name);
        lua_setfield(L, -2, "name");
        if (!list[index].data.is_null()) {
            lua::JsonConverter::push(L, list[index].data);
            lua_setfield(L, -2, "data");
        }

        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }

    return 1;
}

// Returns the track of a property of a target with `track(target, property)`, or nil.
int Clip3DBinding::track(lua_State* L) {
    const std::shared_ptr<Clip3D>& clip = check(L, 1);
    const std::optional<std::size_t> found = clip->findTrack(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<std::string_view>(L, 3));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    Track3DBinding::push(L, std::shared_ptr<const Track3D>(clip, &clip->getTracks()[*found]));
    return 1;
}

// Returns the one-based place in a sample of the first number of the track of a property of a target with `offset(target, property)`, or nil.
int Clip3DBinding::offset(lua_State* L) {
    const Clip3D& clip = *check(L, 1);
    const std::optional<std::size_t> found = clip.findTrack(lua::Stack::read<std::string_view>(L, 2), lua::Stack::read<std::string_view>(L, 3));
    if (!found) {
        lua_pushnil(L);
        return 1;
    }

    lua::Stack::push(L, clip.getValueOffset(*found) + 1);
    return 1;
}

// Writes the value of every track at a time into a float buffer with `sample(time, buffer[, first])`.
int Clip3DBinding::sample(lua_State* L) {
    const Clip3D& clip = *check(L, 1);
    const float time = lua::Stack::read<float>(L, 2);
    const std::span<float> values = lua::core::FloatBufferBinding::readRange(L, 3);
    cursors.assign(clip.getTracks().size(), 0);
    clip.sample(time, values, cursors);
    return 0;
}

int Clip3DBinding::renamed(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->renamed(lua::Stack::read<std::string>(L, 2))));
    return 1;
}

int Clip3DBinding::trimmed(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->trimmed(lua::Stack::read<float>(L, 2), lua::Stack::read<float>(L, 3))));
    return 1;
}

int Clip3DBinding::joined(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->joined(*check(L, 2))));
    return 1;
}

int Clip3DBinding::resampled(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->resampled(lua::Stack::read<float>(L, 2))));
    return 1;
}

int Clip3DBinding::optimized(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->optimized(lua::Stack::read<float>(L, 2))));
    return 1;
}

int Clip3DBinding::compressed(lua_State* L) {
    push(L, std::make_shared<const Clip3D>(check(L, 1)->compressed(lua::Stack::read<float>(L, 2))));
    return 1;
}

// Makes the clip relative to a reference with `toAdditive(reference[, {time}])`, whose pose at `time`, 0 by default, is the base.
int Clip3DBinding::toAdditive(lua_State* L) {
    const Clip3D& clip = *check(L, 1);
    const Clip3D& reference = *check(L, 2);
    float time = 0.0F;
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kAdditiveFields});
        lua::Table::readField(L, 3, "time", time);
    }

    push(L, std::make_shared<const Clip3D>(clip.toAdditive(reference, time)));
    return 1;
}

void Clip3DBinding::install(lua_State* L) {
    lua::ClassBuilder<Clip3D>(L).property("name", &lua::Binding::native<&name>).property("duration", &lua::Binding::native<&duration>).property("valueCount", &lua::Binding::native<&valueCount>).property("memorySize", &lua::Binding::native<&memorySize>).property("tracks", &lua::Binding::native<&tracks>).property("events", &lua::Binding::native<&events>).function("track", &lua::Binding::native<&track>).function("offset", &lua::Binding::native<&offset>).function("sample", &lua::Binding::native<&sample>).function("renamed", &lua::Binding::native<&renamed>).function("trimmed", &lua::Binding::native<&trimmed>).function("joined", &lua::Binding::native<&joined>).function("resampled", &lua::Binding::native<&resampled>).function("optimized", &lua::Binding::native<&optimized>).function("compressed", &lua::Binding::native<&compressed>).function("toAdditive", &lua::Binding::native<&toAdditive>).meta("__eq", &lua::Userdata::equal<Clip3D>).install();
}

} // namespace haylen::lua::animation3d
