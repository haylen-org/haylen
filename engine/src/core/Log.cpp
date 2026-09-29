#include "haylen/core/Log.hpp"

#include <map>
#include <mutex>
#include <string_view>
#include <utility>

#include "varn/log/Log.h"

namespace haylen::core {

// Varn has room for one sink, so this class owns it and fans every line out to the listeners.
struct Log::Listeners {
    std::mutex mutex;
    std::map<std::uint64_t, Listener> entries;
    std::uint64_t next = 1;
    bool installed = false;
};

std::atomic<Log::Level> Log::currentLevel{applyDefaultLevel()};

Log::Listeners& Log::getListeners() {
    static Listeners instance;
    return instance;
}

Log::Level Log::applyDefaultLevel() {
    varn::log::Log::setLevel(toVarnLevel(kDefaultLevel));
    return kDefaultLevel;
}

Log::Level Log::fromVarnLevel(varn::log::Level value) noexcept {
    switch (value) {
    case varn::log::Level::Debug:
        return Level::Debug;
    case varn::log::Level::Info:
        return Level::Info;
    case varn::log::Level::Warn:
        return Level::Warning;
    case varn::log::Level::Error:
        return Level::Error;
    }
    return Level::Info;
}

varn::log::Level Log::toVarnLevel(Level value) noexcept {
    switch (value) {
    case Level::Debug:
        return varn::log::Level::Debug;
    case Level::Info:
        return varn::log::Level::Info;
    case Level::Warning:
        return varn::log::Level::Warn;
    case Level::Error:
        return varn::log::Level::Error;
    }
    return varn::log::Level::Info;
}

// Listeners run under the lock, so a listener that was removed on another thread is never in the middle of a line when removeListener returns.
void Log::dispatch(varn::log::Level level, std::string_view line) {
    Listeners& all = getListeners();
    const std::scoped_lock lock(all.mutex);
    for (const auto& [id, listener] : all.entries) {
        listener(fromVarnLevel(level), line);
    }
}

void Log::setLevel(Level value) noexcept {
    currentLevel.store(value, std::memory_order_relaxed);
    varn::log::Log::setLevel(toVarnLevel(value));
}

Log::Level Log::getLevel() noexcept {
    return currentLevel.load(std::memory_order_relaxed);
}

void Log::write(Level level, std::string_view message) {
    if (level < getLevel()) {
        return;
    }
    varn::log::Log::emit(toVarnLevel(level), message);
}

std::uint64_t Log::addListener(Listener listener) {
    Listeners& all = getListeners();
    const std::scoped_lock lock(all.mutex);
    if (!all.installed) {
        varn::log::Log::setSink(&dispatch);
        all.installed = true;
    }
    const std::uint64_t id = all.next++;
    all.entries.emplace(id, std::move(listener));
    return id;
}

void Log::removeListener(std::uint64_t id) {
    Listeners& all = getListeners();
    const std::scoped_lock lock(all.mutex);
    all.entries.erase(id);
}

} // namespace haylen::core
