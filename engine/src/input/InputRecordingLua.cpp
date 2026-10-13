#include "input/InputRecordingLua.hpp"

#include <lua.hpp>

#include <cmath>
#include <cstdint>
#include <format>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Userdata.hpp"
#include "input/ActionMapLua.hpp"

namespace haylen::input {

std::size_t InputRecordingLua::readPlayer(lua_State* L, int index, std::size_t players) {
    const lua_Integer player = luaL_checkinteger(L, index);
    if (player < 1 || player > static_cast<lua_Integer>(players)) {
        luaL_error(L, "The input recording has players from 1 to %d.", static_cast<int>(players));
    }
    return static_cast<std::size_t>(player - 1);
}

// Creates a recording with `newRecorder({buttons, players = 1, data = ''})`, which keeps the identifier and the fixed rate of the app.
int InputRecordingLua::newRecorder(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    lua::Table::checkFields(L, 1, {kRecorderOptions});
    const core::AppConfig& config = lua::Runtime::getEngine(L).getConfig();
    InputRecording::Header header{.identifier = config.identifier, .fixedRate = config.fixedRate};
    lua::Table::readField(L, 1, "buttons", header.buttons);
    lua::Table::readField(L, 1, "players", header.players);
    lua::Table::readField(L, 1, "data", header.data);
    lua::Userdata::emplace<InputRecording>(L, std::make_shared<InputRecording>(std::move(header)));
    return 1;
}

// Reads the bytes of `recording:bytes()`, which must come from this app at its fixed rate, since a replay at another rate takes other steps.
int InputRecordingLua::loadRecording(lua_State* L) {
    const std::string_view text = lua::Stack::read<std::string_view>(L, 1);
    auto recording = std::make_shared<InputRecording>(InputRecording::fromBytes(std::as_bytes(std::span(text.data(), text.size()))));
    const core::AppConfig& config = lua::Runtime::getEngine(L).getConfig();
    const InputRecording::Header& header = recording->getHeader();
    if (header.identifier != config.identifier) {
        return luaL_error(L, "The recording belongs to another app.");
    }
    if (std::fabs(header.fixedRate - config.fixedRate) > 1e-9) {
        const std::string message = std::format("The recording was made at {} fixed steps a second, and the app runs at {}.", header.fixedRate, config.fixedRate);
        return luaL_error(L, "%s", message.c_str());
    }
    lua::Userdata::emplace<InputRecording>(L, std::move(recording));
    return 1;
}

int InputRecordingLua::newPlayback(lua_State* L) {
    std::shared_ptr<InputRecording> recording = lua::Userdata::checkShared<InputRecording>(L, 1);
    lua::Userdata::emplace<InputPlayback>(L, std::make_shared<InputPlayback>(std::move(recording)));
    return 1;
}

// Writes the mask of a player for the step, given as an integer or read from the step state of an action map for the buttons of the recording.
int InputRecordingLua::write(lua_State* L) {
    InputRecording& recording = lua::Userdata::check<InputRecording>(L, 1);
    const std::size_t player = readPlayer(L, 2, recording.getHeader().players);
    std::uint32_t mask = 0;
    if (const ActionMap* map = lua::Userdata::test<ActionMap>(L, 3)) {
        const std::vector<std::string>& buttons = recording.getHeader().buttons;
        for (std::size_t button = 0; button < buttons.size(); ++button) {
            if (map->isStepDown(buttons[button])) {
                mask |= 1U << button;
            }
        }
    } else {
        const lua_Integer value = luaL_checkinteger(L, 3);
        luaL_argcheck(L, value >= 0 && value <= static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max()), 3, "the buttons of a step are an integer of 32 bits");
        mask = static_cast<std::uint32_t>(value);
    }
    recording.write(player, mask);
    return 0;
}

int InputRecordingLua::advance(lua_State* L) {
    InputRecording& recording = lua::Userdata::check<InputRecording>(L, 1);
    if (lua_isnoneornil(L, 2)) {
        recording.advance();
        return 0;
    }
    const lua_Integer hash = luaL_checkinteger(L, 2);
    luaL_argcheck(L, hash >= 0 && hash <= static_cast<lua_Integer>(std::numeric_limits<std::uint32_t>::max()), 2, "a step hash is an integer of 32 bits");
    recording.advance(static_cast<std::uint32_t>(hash));
    return 0;
}

int InputRecordingLua::bytes(lua_State* L) {
    const std::vector<std::byte> encoded = lua::Userdata::check<InputRecording>(L, 1).toBytes();
    lua_pushlstring(L, reinterpret_cast<const char*>(encoded.data()), encoded.size());
    return 1;
}

int InputRecordingLua::recordingSteps(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<InputRecording>(L, 1).getStepCount()));
    return 1;
}

int InputRecordingLua::recordingPlayers(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<InputRecording>(L, 1).getHeader().players));
    return 1;
}

int InputRecordingLua::recordingButtons(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InputRecording>(L, 1).getHeader().buttons);
    return 1;
}

int InputRecordingLua::recordingData(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InputRecording>(L, 1).getHeader().data);
    return 1;
}

int InputRecordingLua::mask(lua_State* L) {
    const InputPlayback& playback = lua::Userdata::check<InputPlayback>(L, 1);
    lua_pushinteger(L, static_cast<lua_Integer>(playback.getMask(readPlayer(L, 2, playback.getRecording().getHeader().players))));
    return 1;
}

int InputRecordingLua::hash(lua_State* L) {
    const std::optional<std::uint32_t> value = lua::Userdata::check<InputPlayback>(L, 1).getHash();
    if (!value) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, static_cast<lua_Integer>(*value));
    return 1;
}

int InputRecordingLua::play(lua_State* L) {
    lua::Userdata::check<InputPlayback>(L, 1).advance();
    return 0;
}

int InputRecordingLua::seek(lua_State* L) {
    InputPlayback& playback = lua::Userdata::check<InputPlayback>(L, 1);
    const lua_Integer step = luaL_checkinteger(L, 2);
    if (step < 0 || step > static_cast<lua_Integer>(playback.getRecording().getStepCount())) {
        return luaL_error(L, "The playback has steps from 0 to %d.", static_cast<int>(playback.getRecording().getStepCount()));
    }
    playback.seek(static_cast<std::uint32_t>(step));
    return 0;
}

int InputRecordingLua::playbackStep(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<InputPlayback>(L, 1).getStep()));
    return 1;
}

int InputRecordingLua::playbackSteps(lua_State* L) {
    lua_pushinteger(L, static_cast<lua_Integer>(lua::Userdata::check<InputPlayback>(L, 1).getRecording().getStepCount()));
    return 1;
}

int InputRecordingLua::finished(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InputPlayback>(L, 1).isFinished());
    return 1;
}

int InputRecordingLua::playbackData(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<InputPlayback>(L, 1).getRecording().getHeader().data);
    return 1;
}

void InputRecordingLua::install(lua_State* L) {
    lua::ClassBuilder<InputRecording>(L).function("write", &lua::Binding::native<&write>).function("advance", &lua::Binding::native<&advance>).function("bytes", &bytes).property("steps", &recordingSteps).property("players", &recordingPlayers).property("buttons", &recordingButtons).property("data", &recordingData).install();
    lua::ClassBuilder<InputPlayback>(L).function("mask", &lua::Binding::native<&mask>).function("hash", &hash).function("advance", &play).function("seek", &seek).property("step", &playbackStep).property("steps", &playbackSteps).property("finished", &finished).property("data", &playbackData).install();
}

void InputRecordingLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"newRecorder", &lua::Binding::native<&newRecorder>},
        {"loadRecording", &lua::Binding::native<&loadRecording>},
        {"newPlayback", &newPlayback},
        {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::input
