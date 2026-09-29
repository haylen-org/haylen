#pragma once

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/storage/Preferences.hpp"
#include "haylen/storage/SaveSlots.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::plugins {

// Owns the save slots and preferences of the app and exposes user storage to Lua as haylen.storage and haylen.preferences. Preferences load when the engine starts, and unsaved changes are written when the app goes to the background or stops. Background storage operations run on the I/O pool one at a time, in the order they were queued.
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

    // Runs the operation on the I/O pool once every operation queued before it finished, so operations on the same files never overlap and reach the disk in order. The operation must not throw, and it hands its result back itself, such as through the frame queue of the job system. Stopping the plugin runs the operations still waiting on the frame thread, so a save queued just before the app quits still reaches the disk.
    void queueOperation(std::function<void()> operation);

    // Stores the volume and mute state of every audio bus, the fullscreen state and the action map under audio.volume.<bus>, audio.muted.<bus>, window.fullscreen and input.actions.
    void captureEnginePreferences(core::Engine& engine);

    // Applies whichever of those keys are stored. Buses the app has not created are left alone, and a stored action map replaces the current one.
    void applyEnginePreferences(core::Engine& engine);

  private:
    // The operations waiting for the I/O pool, shared with the pool jobs that run them. Running marks the thread that runs them now, and scheduled a pool job that will.
    struct Operations {
        std::mutex mutex;
        std::condition_variable idle;
        std::deque<std::function<void()>> waiting;
        bool scheduled = false;
        bool running = false;
    };

    // Runs the waiting operations one at a time, after the thread that runs them already, if any, finished.
    static void runOperations(Operations& queue);

    // Writes pending preferences from shutdown paths, where a failure must not escape.
    static void saveQuietly(storage::Preferences& target) noexcept;

    // Returns the stored preference, or null when it is absent, and rejects a stored value of another JSON type.
    [[nodiscard]] static core::Json getStoredPreference(const storage::Preferences& source, const std::string& key, core::Json::value_t type);

    std::shared_ptr<Operations> operations = std::make_shared<Operations>();
    core::JobSystem* jobs = nullptr;
    std::unique_ptr<storage::SaveSlots> saveSlots;
    std::unique_ptr<storage::Preferences> preferences;
    core::ScopedConnection appStateConnection;
};

} // namespace haylen::plugins
