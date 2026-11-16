#pragma once

#include <memory>

#include "haylen/lua/Reference.hpp"
#include "haylen/ui/Screen.hpp"

struct lua_State;

namespace haylen::ui {

// A screen implemented by a Lua table, whose `build` method returns the node table of its GUI and whose optional methods mirror the other hooks of a screen. A table is one screen for as long as the engine holds it. The engine sets the field `gui` of the table to its GUI once it is built, and the field `stack` or `presentation` to what holds it, and everything the table owns ends when the screen is removed.
class ScriptedScreen final : public Screen {
  public:
    ScriptedScreen(lua_State* L, int index);

    // Returns the screen of the table at index, creating it when the engine holds none. Raises a Lua error for a value that is not a table.
    [[nodiscard]] static std::shared_ptr<ScriptedScreen> get(lua_State* L, int index);

    // Returns the state of the screen of the table at index without making one: the state of the screen the engine holds, `Removed` for a table whose screen left, or `Created` for a table that never was a screen.
    [[nodiscard]] static State findState(lua_State* L, int index);

    // Pushes the table of a screen made from Lua, or `nil` for a screen of C++.
    static void push(lua_State* L, const std::shared_ptr<Screen>& screen);

    [[nodiscard]] std::shared_ptr<Gui> build(core::Engine& engine, Placement placement) override;
    void appear(core::Engine& engine) override;
    void appeared(core::Engine& engine) override;
    void disappear(core::Engine& engine) override;
    void disappeared(core::Engine& engine) override;
    void update(core::Engine& engine, float deltaSeconds) override;
    void render(core::Engine& engine) override;
    [[nodiscard]] bool back(core::Engine& engine) override;
    void remove(core::Engine& engine) override;

  protected:
    void placed(core::Engine& engine) override;

  private:
    // Maps screen tables to their screens with weak keys, so the map holds neither.
    static constexpr const char* kScreens = "haylen.screens";
    static constexpr const char* kHandleType = "haylen.ScreenHandle";

    static int collectHandle(lua_State* L);

    // Calls the method of the table named `name`, when it has one, with the table as `self` and the number as its argument when one is given, and returns whether the call returned true.
    bool call(const char* name, const float* argument = nullptr) const;

    lua::Reference table;
};

} // namespace haylen::ui
