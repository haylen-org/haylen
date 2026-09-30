#pragma once

#include <lua.hpp>

#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/TweenProperty.hpp"
#include "haylen/core/TweenTrack.hpp"
#include "haylen/core/TweenValue.hpp"
#include "lua/WeakReference.hpp"

namespace haylen::core {

// A tween track that writes fields of a Lua table or object by plain assignment: fields named by a path such as `position.x`, and pairs of number fields that together hold one `Vec2`, such as `x` and `y`. It holds the target weakly and every read and write runs in one protected call per frame.
class ScriptedTrack final : public TweenTrack {
  public:
    ScriptedTrack(lua_State* L, int object);

    // The parts of a field name such as `position.x`.
    using Path = std::vector<std::string>;

    // Adds a value read from and written to one field, or to two number fields that hold a `Vec2`.
    void add(const std::vector<std::string>& names, TweenProperty property);

    // Reads the current value of the fields from the target at index.
    [[nodiscard]] static TweenValue read(lua_State* L, int object, const std::vector<Path>& paths);
    [[nodiscard]] static std::vector<Path> split(const std::vector<std::string>& names);

    void begin() override;
    void render(float progress, int loops) override;
    [[nodiscard]] bool isAlive() const override;
    [[nodiscard]] float getDistance() const override;
    [[nodiscard]] const void* getTarget() const noexcept override {
        return target.getIdentity();
    }
    [[nodiscard]] std::vector<std::string> getFields() const override;
    bool release(std::string_view field) override;

  private:
    struct Field {
        std::vector<std::string> names;
        std::vector<Path> paths;
        TweenProperty property;
        bool released = false;
    };

    static void pushPath(lua_State* L, int object, const Path& path);
    [[nodiscard]] static TweenValue toValue(lua_State* L, int index, const Path& path);
    static void pushValue(lua_State* L, const TweenValue& value);
    static void write(lua_State* L, int object, const Path& path, const TweenValue& value);
    static void checkIndexable(lua_State* L, int index, const Path& path);
    [[nodiscard]] static std::string join(const Path& path);

    lua_State* state;
    lua::WeakReference target;
    std::vector<Field> fields;
};

} // namespace haylen::core
