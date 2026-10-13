#pragma once

#include <array>
#include <string_view>

struct lua_State;

namespace haylen::platform {

struct AccessibilityElement;

// Installs `haylen.accessibility`, which reads the accessibility settings of the system, says announcements and reads and acts on the elements that screen readers read.
class AccessibilityLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 1> kAnnounceOptions{"interrupt"};

    static void pushElement(lua_State* L, const AccessibilityElement& element);

    static int settings(lua_State* L);
    static int announce(lua_State* L);
    static int elements(lua_State* L);
    static int focused(lua_State* L);
    static int perform(lua_State* L);
    static int setTreeEnabled(lua_State* L);
    static int treeEnabled(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
