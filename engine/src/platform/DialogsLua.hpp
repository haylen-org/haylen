#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "haylen/platform/DialogRequest.hpp"
#include "haylen/platform/DialogResult.hpp"

struct lua_State;

namespace haylen::platform {

// Installs `haylen.dialogs`, which shows native message boxes and pickers of files and folders. Each function returns a call like the ones of `platform.call`, whose `await` gives the choice of the user.
class DialogsLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kMessageOptions{"title", "text", "kind", "buttons", "timeout"};
    static constexpr std::array<std::string_view, 4> kOpenFilesOptions{"title", "filters", "multiple", "timeout"};
    static constexpr std::array<std::string_view, 5> kSaveFileOptions{"title", "filters", "name", "data", "timeout"};
    static constexpr std::array<std::string_view, 2> kOpenFolderOptions{"title", "timeout"};
    static constexpr std::array<std::string_view, 2> kFilterFields{"name", "extensions"};

    // Shows the dialog with the timeout of the options at the given stack index and pushes the call that settles with its answer.
    static void pushCall(lua_State* L, const DialogRequest& request, int options);

    [[nodiscard]] static std::optional<std::chrono::steady_clock::duration> readTimeout(lua_State* L, int options);
    [[nodiscard]] static std::vector<DialogRequest::Filter> readFilters(lua_State* L, int options);

    // Pushes the choice of the user as Lua sees it: the button counted from one, the list of opened files, the saved file, the folder, or `nil` when the user dismissed the dialog.
    static void pushChoice(lua_State* L, const DialogResult& result);
    static void pushFile(lua_State* L, const DialogResult::File& file);

    [[nodiscard]] static bool cancelDialog(lua_State* L, std::uint64_t id);

    static int message(lua_State* L);
    static int openFiles(lua_State* L);
    static int saveFile(lua_State* L);
    static int openFolder(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::platform
