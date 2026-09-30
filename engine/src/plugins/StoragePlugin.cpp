#include "haylen/plugins/StoragePlugin.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/JsonNumber.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/input/ActionMap.hpp"
#include "haylen/platform/Window.hpp"
#include "storage/PreferencesLua.hpp"
#include "storage/StorageLua.hpp"

namespace haylen::plugins {

void StoragePlugin::saveQuietly(storage::Preferences& target) noexcept {
    try {
        if (target.isDirty()) {
            target.save();
        }
    } catch (const std::exception& error) {
        core::Log::error("The preferences could not be saved: {}", error.what());
    }
}

core::Json StoragePlugin::getStoredPreference(const storage::Preferences& source, const std::string& key, core::Json::value_t type) {
    core::Json value = source.get(key);
    const bool numeric = type == core::Json::value_t::number_float && value.is_number();
    if (!value.is_null() && !numeric && value.type() != type) {
        throw std::invalid_argument("The preference \"" + key + "\" has a value of the wrong type.");
    }
    return value;
}

void StoragePlugin::start(core::Engine& engine) {
    jobs = &engine.getJobs();
    saveSlots = std::make_unique<storage::SaveSlots>(engine.getStorage());
    preferences = std::make_unique<storage::Preferences>(engine.getStorage());
    try {
        preferences->load();
    } catch (const std::exception& error) {
        core::Log::warning("{} The app starts with default preferences, and the next save replaces the file.", error.what());
    }

    // clang-format off
    appStateConnection = engine.appStateChanged.connect([this](core::Engine::AppState value) {
        if (value == core::Engine::AppState::Background && preferences) {
            saveQuietly(*preferences);
        }
    });
    // clang-format on
}

void StoragePlugin::stop(core::Engine&) {
    appStateConnection.disconnect();
    runOperations(*operations);
    jobs = nullptr;
    if (preferences) {
        saveQuietly(*preferences);
    }
}

void StoragePlugin::queueOperation(std::function<void()> operation) {
    if (jobs == nullptr) {
        throw std::logic_error("The storage plugin has not started.");
    }
    bool schedule = false;
    {
        const std::scoped_lock lock(operations->mutex);
        operations->waiting.push_back(std::move(operation));
        schedule = !std::exchange(operations->scheduled, true);
    }
    if (schedule) {
        jobs->postIo([queue = operations] { runOperations(*queue); });
    }
}

void StoragePlugin::runOperations(Operations& queue) {
    std::unique_lock lock(queue.mutex);
    queue.idle.wait(lock, [&queue] { return !queue.running; });
    queue.running = true;
    while (!queue.waiting.empty()) {
        std::function<void()> operation = std::move(queue.waiting.front());
        queue.waiting.pop_front();
        lock.unlock();
        operation();
        lock.lock();
    }
    queue.running = false;
    queue.scheduled = false;
    queue.idle.notify_all();
}

void StoragePlugin::installLua(core::Engine&, lua_State* L) {
    storage::StorageLua::install(L);
    storage::PreferencesLua::install(L);
}

storage::SaveSlots& StoragePlugin::getSaveSlots() {
    if (!saveSlots) {
        throw std::logic_error("The storage plugin has not started.");
    }
    return *saveSlots;
}

storage::Preferences& StoragePlugin::getPreferences() {
    if (!preferences) {
        throw std::logic_error("The storage plugin has not started.");
    }
    return *preferences;
}

void StoragePlugin::captureEnginePreferences(core::Engine& engine) {
    storage::Preferences& values = getPreferences();
    audio::Mixer& mixer = engine.getAudio();
    for (const std::string& bus : mixer.getBuses()) {
        values.set("audio.volume." + bus, core::JsonNumber::fromFloat(mixer.getBusVolume(bus)));
        values.set("audio.muted." + bus, mixer.isBusMuted(bus));
    }
    values.set("window.fullscreen", engine.getWindow().isFullscreen());
    values.set("input.actions", engine.getActions().save());
}

void StoragePlugin::applyEnginePreferences(core::Engine& engine) {
    const storage::Preferences& values = getPreferences();
    audio::Mixer& mixer = engine.getAudio();
    for (const std::string& bus : mixer.getBuses()) {
        const core::Json volume = getStoredPreference(values, "audio.volume." + bus, core::Json::value_t::number_float);
        if (!volume.is_null()) {
            mixer.setBusVolume(bus, volume.get<float>());
        }
        const core::Json muted = getStoredPreference(values, "audio.muted." + bus, core::Json::value_t::boolean);
        if (!muted.is_null()) {
            mixer.setBusMuted(bus, muted.get<bool>());
        }
    }

    const core::Json fullscreen = getStoredPreference(values, "window.fullscreen", core::Json::value_t::boolean);
    if (!fullscreen.is_null()) {
        engine.getWindow().setFullscreen(fullscreen.get<bool>());
    }
    const core::Json actions = getStoredPreference(values, "input.actions", core::Json::value_t::object);
    if (!actions.is_null()) {
        engine.getActions().load(actions);
    }
}

} // namespace haylen::plugins
