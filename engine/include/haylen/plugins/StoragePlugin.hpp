#pragma once

#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "haylen/storage/Preferences.hpp"
#include "haylen/storage/SaveSlots.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::plugins {

// Owns the save slots and preferences of the app and exposes user storage to Lua as `haylen.storage` and `haylen.preferences`. Preferences load when the engine starts, and unsaved changes are written when the app stops and when it goes to the background, after the listeners of `appBackground` have run. Background storage operations run on the I/O pool one at a time, in the order they were queued.
class StoragePlugin final : public Plugin {
  public:
    [[nodiscard]] std::string_view getName() const noexcept override {
        return "storage";
    }
    void start(core::Engine& engine) override;
    void stop(core::Engine& engine) override;
    void event(core::Engine& engine, const platform::Event& event) override;
    void installLua(core::Engine& engine, lua_State* L) override;

    [[nodiscard]] storage::SaveSlots& getSaveSlots();
    [[nodiscard]] storage::Preferences& getPreferences();

    // Runs the operation on the I/O pool once every operation queued before it finished, so operations on the same files never overlap and reach the disk in order. The operation hands its result back itself, such as through the frame queue of the job system, and an exception it throws reaches the engine as an error while the operations after it still run. Stopping the plugin runs the operations still waiting on the frame thread, so a save queued just before the app quits still reaches the disk.
    void queueOperation(std::function<void()> operation);

    // Stores the volume and mute state of every audio bus in the groups `audio.volume` and `audio.muted`, whose keys are the bus names, the fullscreen state under `window.fullscreen` and the action map under `input.actions`.
    void captureEnginePreferences(core::Engine& engine);

    // Applies whichever of those values are stored. Buses the app has not created are left alone, and a stored action map replaces the current one.
    void applyEnginePreferences(core::Engine& engine);

  private:
    // The operations waiting for the I/O pool, shared with the pool jobs that run them. The flag `running` marks the thread that runs them now, and `scheduled` a pool job that will.
    struct Operations {
        std::mutex mutex;
        std::condition_variable idle;
        std::deque<std::function<void()>> waiting;
        bool scheduled = false;
        bool running = false;
    };

    // Runs the waiting operations one at a time, after the thread that runs them already, if any, finished. An operation that throws does not stop the others, and the first failure is returned once the queue is idle.
    [[nodiscard]] static std::exception_ptr runOperations(Operations& queue);

    // Writes pending preferences from shutdown paths, where a failure must not escape.
    static void saveQuietly(storage::Preferences& target) noexcept;

    // Tells whether a stored value is absent or of the JSON type, where any number counts as a float.
    [[nodiscard]] static bool hasStoredType(const core::Json& value, core::Json::value_t type) noexcept;

    // Returns the stored preference, or null when it is absent, and rejects a stored value of another JSON type.
    [[nodiscard]] static core::Json getStoredPreference(const storage::Preferences& source, const std::string& key, core::Json::value_t type);

    // Returns the value of the bus in the group stored under the key, or null when it is absent, and rejects a value of another JSON type.
    [[nodiscard]] static core::Json getStoredBusPreference(const core::Json& group, const std::string& key, const std::string& bus, core::Json::value_t type);

    std::shared_ptr<Operations> operations = std::make_shared<Operations>();
    core::JobSystem* jobs = nullptr;
    std::unique_ptr<storage::SaveSlots> saveSlots;
    std::unique_ptr<storage::Preferences> preferences;
};

} // namespace haylen::plugins
