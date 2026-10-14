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
#include "haylen/platform/Event.hpp"
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

bool StoragePlugin::hasStoredType(const core::Json& value, core::Json::value_t type) noexcept {
    return value.is_null() || value.type() == type || (type == core::Json::value_t::number_float && value.is_number());
}

core::Json StoragePlugin::getStoredPreference(const storage::Preferences& source, const std::string& key, core::Json::value_t type) {
    core::Json value = source.get(key);
    if (!hasStoredType(value, type)) {
        throw std::invalid_argument("The preference \"" + key + "\" has a value of the wrong type.");
    }
    return value;
}

core::Json StoragePlugin::getStoredBusPreference(const core::Json& group, const std::string& key, const std::string& bus, core::Json::value_t type) {
    const auto found = group.find(bus);
    if (found == group.end()) {
        return nullptr;
    }
    if (!hasStoredType(*found, type)) {
        throw std::invalid_argument("The preference \"" + key + "\" has a value of the wrong type for the bus \"" + bus + "\".");
    }
    return *found;
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
    resumeState = std::make_unique<storage::ResumeState>(engine.getStorage());
    try {
        resumeState->load();
    } catch (const std::exception& error) {
        core::Log::warning("{}", error.what());
    }
}

// The engine handles the suspension before its plugins, so the listeners of `appBackground` have run, and what they changed is saved with the rest.
void StoragePlugin::event(core::Engine&, const platform::Event& event) {
    if (!preferences) {
        return;
    }
    if (event.type == platform::Event::Type::Suspended) {
        saveQuietly(*preferences);
        writeResumeState(true);
    } else if (event.type == platform::Event::Type::Resumed) {
        writeResumeState(false);
    }
}

// The app goes on whether the resume state reached the disk or not, as it would without one.
void StoragePlugin::writeResumeState(bool background) noexcept {
    try {
        if (background) {
            resumeState->save();
        } else {
            resumeState->discard();
        }
    } catch (const std::exception& error) {
        core::Log::error("The resume state could not be written: {}", error.what());
    }
}

// The preferences are saved even when a waiting operation failed, whose error reaches the engine afterwards.
void StoragePlugin::stop(core::Engine&) {
    const std::exception_ptr failure = runOperations(*operations);
    jobs = nullptr;
    if (preferences) {
        saveQuietly(*preferences);
        writeResumeState(false);
    }
    if (failure) {
        std::rethrow_exception(failure);
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
        // clang-format off
        jobs->postIo([queue = operations] {
            if (const std::exception_ptr failure = runOperations(*queue)) {
                std::rethrow_exception(failure);
            }
        });
        // clang-format on
    }
}

std::exception_ptr StoragePlugin::runOperations(Operations& queue) {
    std::exception_ptr failure;
    std::unique_lock lock(queue.mutex);
    queue.idle.wait(lock, [&queue] { return !queue.running; });
    queue.running = true;
    while (!queue.waiting.empty()) {
        std::function<void()> operation = std::move(queue.waiting.front());
        queue.waiting.pop_front();
        lock.unlock();
        try {
            operation();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
        lock.lock();
    }
    queue.running = false;
    queue.scheduled = false;
    queue.idle.notify_all();
    return failure;
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

storage::ResumeState& StoragePlugin::getResumeState() {
    if (!resumeState) {
        throw std::logic_error("The storage plugin has not started.");
    }
    return *resumeState;
}

// Bus names may hold dots, so every bus is a key of one group instead of a part of a dotted path, and buses the app has not created keep what the group stored for them.
void StoragePlugin::captureEnginePreferences(core::Engine& engine) {
    storage::Preferences& values = getPreferences();
    audio::Mixer& mixer = engine.getAudio();
    core::Json volumes = getStoredPreference(values, "audio.volume", core::Json::value_t::object);
    core::Json muted = getStoredPreference(values, "audio.muted", core::Json::value_t::object);
    for (const std::string& bus : mixer.getBuses()) {
        volumes[bus] = core::JsonNumber::fromFloat(mixer.getBusVolume(bus));
        muted[bus] = mixer.isBusMuted(bus);
    }
    values.set("audio.volume", std::move(volumes));
    values.set("audio.muted", std::move(muted));
    values.set("window.fullscreen", engine.getWindow().isFullscreen());
    values.set("input.actions", engine.getActions().save());
}

void StoragePlugin::applyEnginePreferences(core::Engine& engine) {
    const storage::Preferences& values = getPreferences();
    audio::Mixer& mixer = engine.getAudio();
    const core::Json volumes = getStoredPreference(values, "audio.volume", core::Json::value_t::object);
    const core::Json muted = getStoredPreference(values, "audio.muted", core::Json::value_t::object);
    for (const std::string& bus : mixer.getBuses()) {
        const core::Json volume = getStoredBusPreference(volumes, "audio.volume", bus, core::Json::value_t::number_float);
        if (!volume.is_null()) {
            mixer.setBusVolume(bus, volume.get<float>());
        }
        const core::Json mute = getStoredBusPreference(muted, "audio.muted", bus, core::Json::value_t::boolean);
        if (!mute.is_null()) {
            mixer.setBusMuted(bus, mute.get<bool>());
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
