#pragma once

#include <lua.hpp>

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::debug {

// What the person looks at while the debugger halts the app: the levels of the stack of the coroutine that halted, the scopes of each level and the values inside them, which the client names by the numbers that this view hands out, as the Debug Adapter Protocol describes them. Looking reads raw values only, so it never runs a metamethod of the app, and the values it handed out stay in a table of the registry until the app goes on.
class HaltedStack final {
  public:
    // The longest text of a string value that a variable shows.
    static constexpr std::size_t kMaxText = 1000;

    // The most values that one expansion lists, which the client pages through beyond that.
    static constexpr int kMaxChildren = 1000;

    // A view of the stack of the coroutine from the level on, which is 0 for the function that runs and 1 when a native function of the engine halted it, such as a message handler.
    HaltedStack(lua_State* thread, int firstLevel);
    ~HaltedStack();

    HaltedStack(const HaltedStack&) = delete;
    HaltedStack& operator=(const HaltedStack&) = delete;

    // The body of a `stackTrace` response from the frame at the index, with at most the count of frames, or every frame from there with a count of 0.
    [[nodiscard]] core::Json getFrames(int start, int levels) const;

    // The body of a `scopes` response, or null for a frame that does not exist.
    [[nodiscard]] core::Json getScopes(int frameId);

    // The body of a `variables` response, or null for a reference that this halt never handed out.
    [[nodiscard]] core::Json getVariables(int reference, int start, int count);

    // Pushes the environment that an evaluation in the frame sees: its locals, then its upvalues, then the globals of its function, which assignments to other names reach. Returns false and pushes nothing for a frame that does not exist.
    [[nodiscard]] bool pushEnvironment(int frameId);

    // Describes the value at the index as a variable with the name, as the `variables` and `evaluate` responses list it, with a reference for a value that holds more values.
    [[nodiscard]] core::Json describe(const std::string& name, int index);

    // The text that a variable shows for the value at the index.
    [[nodiscard]] static std::string formatValue(lua_State* L, int index);

  private:
    enum class Kind : std::uint8_t {
        Locals,
        Upvalues,
        Globals,
        Value,
    };

    // What a reference names: the scope of a level or a value of the registry table.
    struct Handle {
        Kind kind = Kind::Value;
        int level = 0;
        int slot = 0;
    };

    // One value that an expansion lists, with what it sorts by: integer keys first in order, then the others by their name.
    struct Child {
        bool numbered = false;
        lua_Integer number = 0;
        std::string name;
        core::Json variable;
    };

    [[nodiscard]] bool findLevel(int frameId, lua_Debug& level) const;
    [[nodiscard]] int addHandle(Handle handle);
    [[nodiscard]] static std::string describeFunction(const lua_Debug& level);
    [[nodiscard]] static std::string describeKey(lua_State* L, int index);
    [[nodiscard]] static std::string quote(std::string_view text);

    // Pushes the `_ENV` of the function at the index, or the global table when it has none.
    static void pushGlobals(lua_State* L, int function);

    void listLocals(const Handle& handle, std::vector<Child>& children);
    void listUpvalues(int function, std::vector<Child>& children);
    void listTable(int table, std::vector<Child>& children);
    void listUserdata(int userdata, std::vector<Child>& children);
    void listProperties(int userdata, int getters, std::vector<Child>& children);

    // The metamethods of the environment of an evaluation, which tell the names of the frame from the globals.
    static int readName(lua_State* L);
    static int writeName(lua_State* L);

    lua_State* state;
    int first;
    int values = LUA_NOREF;
    int valueCount = 0;
    std::vector<Handle> handles;
};

} // namespace haylen::debug
