#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "haylen/core/Connection.hpp"
#include "haylen/input/GamepadMotion.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::input {

class Input;

// Installs the `GamepadMotionRequest` class of `haylen.input` and the functions of the module that run the motion sensors of gamepads and read them and their touchpads.
class GamepadMotionLua final {
  public:
    // A request that runs, which Lua holds to stop it and to read what it runs.
    struct Request {
        std::uint64_t id = 0;
        GamepadMotion::Sensors sensors;
    };

    static void install(lua_State* L);

    // Sets the functions of the motion of gamepads on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    // Ends a request when its owner ends.
    class Ownership final : public core::Connection::Link {
      public:
        Ownership(GamepadMotion& owner, std::uint64_t request) noexcept : motion(owner), id(request) {}

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return connected;
        }
        void setBlocked(bool) override {}
        [[nodiscard]] bool isBlocked() const noexcept override {
            return false;
        }

      private:
        GamepadMotion& motion;
        std::uint64_t id;
        bool connected = true;
    };

    static constexpr std::array<std::string_view, 2> kStartOptions{"sensors", "owner"};

    [[nodiscard]] static Input& getInput(lua_State* L);
    [[nodiscard]] static std::size_t readGamepadIndex(lua_State* L, int index);
    static void readSensors(lua_State* L, int index, GamepadMotion::Sensors& sensors);
    static void pushSensors(lua_State* L, const GamepadMotion::Sensors& sensors);
    static int pushVector(lua_State* L, const std::array<float, 3>& value);

    static int startGamepadMotion(lua_State* L);
    static int gamepadSensors(lua_State* L);
    static int gamepadAcceleration(lua_State* L);
    static int gamepadRotationRate(lua_State* L);
    static int gamepadAttitude(lua_State* L);
    static int gamepadTilt(lua_State* L);
    static int gamepadTouches(lua_State* L);

    static int stop(lua_State* L);
    static int running(lua_State* L);
    static int sensors(lua_State* L);
};

} // namespace haylen::input

namespace haylen::lua {

template <> struct Type<input::GamepadMotionLua::Request> {
    static constexpr const char* name = "haylen.GamepadMotionRequest";
    using Storage = input::GamepadMotionLua::Request;
};

} // namespace haylen::lua
