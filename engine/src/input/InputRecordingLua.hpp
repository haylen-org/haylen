#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string_view>

#include "haylen/input/InputPlayback.hpp"
#include "haylen/input/InputRecording.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<input::InputRecording> {
    static constexpr const char* name = "haylen.InputRecording";
    using Storage = std::shared_ptr<input::InputRecording>;
};

template <> struct Type<input::InputPlayback> {
    static constexpr const char* name = "haylen.InputPlayback";
    using Storage = std::shared_ptr<input::InputPlayback>;
};

} // namespace haylen::lua

namespace haylen::input {

// Installs the `InputRecording` and `InputPlayback` classes of `haylen.input` with `input.newRecorder`, `input.loadRecording` and `input.newPlayback`. Players count from 1 in Lua.
class InputRecordingLua final {
  public:
    static void install(lua_State* L);

    // Sets the functions of recordings on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kRecorderOptions{"buttons", "players", "data"};

    [[nodiscard]] static std::size_t readPlayer(lua_State* L, int index, std::size_t players);

    static int newRecorder(lua_State* L);
    static int loadRecording(lua_State* L);
    static int newPlayback(lua_State* L);
    static int write(lua_State* L);
    static int advance(lua_State* L);
    static int bytes(lua_State* L);
    static int recordingSteps(lua_State* L);
    static int recordingPlayers(lua_State* L);
    static int recordingButtons(lua_State* L);
    static int recordingData(lua_State* L);
    static int mask(lua_State* L);
    static int hash(lua_State* L);
    static int play(lua_State* L);
    static int seek(lua_State* L);
    static int playbackStep(lua_State* L);
    static int playbackSteps(lua_State* L);
    static int finished(lua_State* L);
    static int playbackData(lua_State* L);
};

} // namespace haylen::input
