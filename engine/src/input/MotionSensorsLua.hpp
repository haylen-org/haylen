#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "haylen/core/Connection.hpp"
#include "haylen/input/MotionSensors.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::input {

class Input;

// Installs the `MotionRequest` class of `haylen.input` and the functions of the module that start the motion sensors and read them and the touchpads of gamepads.
class MotionSensorsLua final {
  public:
    // A request that started, which Lua holds to stop it and to read what it runs.
    struct Request {
        std::uint64_t id = 0;
        MotionSensors::Options options;
    };

    static void install(lua_State* L);

    // Sets the functions of the motion sensors on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    // Ends a request when its owner ends.
    class Ownership final : public core::Connection::Link {
      public:
        Ownership(MotionSensors& owner, std::uint64_t request) noexcept : sensors(owner), id(request) {}

        void disconnect() override;
        [[nodiscard]] bool isConnected() const noexcept override {
            return connected;
        }
        void setBlocked(bool) override {}
        [[nodiscard]] bool isBlocked() const noexcept override {
            return false;
        }

      private:
        MotionSensors& sensors;
        std::uint64_t id;
        bool connected = true;
    };

    static constexpr std::array<std::string_view, 5> kStartOptions{"sensors", "rate", "device", "gamepads", "owner"};

    [[nodiscard]] static Input& getInput(lua_State* L);
    [[nodiscard]] static std::size_t readGamepadIndex(lua_State* L, int index);
    static void readSensors(lua_State* L, int index, MotionSensors::Sensors& sensors);
    static void pushSensors(lua_State* L, const MotionSensors::Sensors& sensors);
    static int pushVector(lua_State* L, const std::array<float, 3>& value);
    static int pushQuaternion(lua_State* L, const std::array<float, 4>& value);
    [[nodiscard]] static bool cancelStart(lua_State* L, std::uint64_t id);

    static int startMotion(lua_State* L);
    static int motionSensors(lua_State* L);
    static int acceleration(lua_State* L);
    static int rotationRate(lua_State* L);
    static int attitude(lua_State* L);
    static int tilt(lua_State* L);
    static int calibrateTilt(lua_State* L);
    static int resetTilt(lua_State* L);
    static int gamepadSensors(lua_State* L);
    static int gamepadAcceleration(lua_State* L);
    static int gamepadRotationRate(lua_State* L);
    static int gamepadAttitude(lua_State* L);
    static int gamepadTilt(lua_State* L);
    static int gamepadTouches(lua_State* L);

    static int stop(lua_State* L);
    static int running(lua_State* L);
    static int sensors(lua_State* L);
    static int rate(lua_State* L);
    static int device(lua_State* L);
    static int gamepads(lua_State* L);
};

} // namespace haylen::input

namespace haylen::lua {

template <> struct Type<input::MotionSensorsLua::Request> {
    static constexpr const char* name = "haylen.MotionRequest";
    using Storage = input::MotionSensorsLua::Request;
};

} // namespace haylen::lua
