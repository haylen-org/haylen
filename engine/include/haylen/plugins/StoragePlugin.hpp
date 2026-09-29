#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/storage/Preferences.hpp"
#include "haylen/storage/SaveSlots.hpp"

namespace haylen::plugins {

// Owns the save slots and preferences of the app and exposes user storage to Lua as haylen.storage and haylen.preferences. Preferences load when the engine starts, and unsaved changes are written when the app goes to the background or stops.
class StoragePlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "storage";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    [[nodiscard]] storage::SaveSlots& getSaveSlots();
    [[nodiscard]] storage::Preferences& getPreferences();

    // Stores the volume and mute state of every audio bus, the fullscreen state and the action map under audio.volume.<bus>, audio.muted.<bus>, window.fullscreen and input.actions.
    void captureEnginePreferences(core::Engine& engine);

    // Applies whichever of those keys are stored. Buses the app has not created are left alone, and a stored action map replaces the current one.
    void applyEnginePreferences(core::Engine& engine);

  private:
    // Writes pending preferences from shutdown paths, where a failure must not escape.
    static void saveQuietly(storage::Preferences& target) noexcept;

    // Returns the stored preference, or null when it is absent, and rejects a stored value of another JSON type.
    [[nodiscard]] static core::Json getStoredPreference(const storage::Preferences& source, const std::string& key, core::Json::value_t type);

    std::unique_ptr<storage::SaveSlots> saveSlots;
    std::unique_ptr<storage::Preferences> preferences;
    core::ScopedConnection appStateConnection;
};

} // namespace haylen::plugins
