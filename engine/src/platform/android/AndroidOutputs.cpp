#include "platform/android/AndroidOutputs.hpp"

#include <algorithm>
#include <exception>
#include <utility>

#include "haylen/core/Log.hpp"
#include "platform/Player.hpp"
#include "platform/android/JavaBridge.hpp"

namespace haylen::platform {

std::mutex& AndroidOutputs::mutex = *new std::mutex();
std::condition_variable& AndroidOutputs::released = *new std::condition_variable();
std::unordered_map<std::uint32_t, AndroidOutputs::Entry>& AndroidOutputs::entries = *new std::unordered_map<std::uint32_t, Entry>();
std::vector<AndroidOutputs::Retired>& AndroidOutputs::retiring = *new std::vector<Retired>();
std::uint64_t AndroidOutputs::retiredSequence = 0;
std::uint64_t AndroidOutputs::frameSequence = 0;
std::uint64_t AndroidOutputs::releasedSequence = 0;

void AndroidOutputs::open(std::uint32_t number, std::string_view display, const Output::Options& options) {
    {
        const std::scoped_lock lock(mutex);
        entries[number] = {};
    }
    JavaBridge::openOutput(number, display, options.title);
}

// The engine let go of the surface before it closes the output, so the window goes at once.
void AndroidOutputs::close(std::uint32_t number) {
    {
        const std::scoped_lock lock(mutex);
        const auto found = entries.find(number);
        if (found == entries.end()) {
            return;
        }
        Retired gone{.window = found->second.window, .drawable = std::move(found->second.drawable)};
        release(gone);
        entries.erase(found);
    }
    JavaBridge::closeOutput(number);
}

// A window shows the output through the device that draws into it directly, or through a drawable of the EGL context that the report keeps while the window lasts.
Host::OutputReport AndroidOutputs::getReport(std::uint32_t number, const AndroidGraphics& graphics) {
    const std::scoped_lock lock(mutex);
    const auto found = entries.find(number);
    if (found == entries.end()) {
        return {.ending = Output::Ending::App};
    }
    Entry& entry = found->second;
    if (entry.ending) {
        return {.ending = entry.ending, .reason = entry.reason};
    }
    if (entry.window == nullptr) {
        return {};
    }

    try {
        gpu::Surface surface = entry.drawable != nullptr ? gpu::Surface{.kind = gpu::Surface::Kind::Gl, .gl = entry.drawable.get()} : graphics.describeOutput(*entry.window, entry.width, entry.height, entry.drawable);
        surface.width = entry.width;
        surface.height = entry.height;
        return {.surface = surface, .scale = entry.scale};
    } catch (const std::exception& error) {
        entry.ending = Output::Ending::Unsupported;
        entry.reason = error.what();
        return {.ending = entry.ending, .reason = entry.reason};
    }
}

void AndroidOutputs::beginFrame() noexcept {
    const std::scoped_lock lock(mutex);
    frameSequence = retiredSequence;
}

// The frame read the end of every window that went before it began, and the device let go of their surfaces.
void AndroidOutputs::endFrame() noexcept {
    {
        const std::scoped_lock lock(mutex);
        const auto settled = std::ranges::partition(retiring, [](const Retired& retired) { return retired.sequence > frameSequence; });
        for (Retired& retired : settled) {
            release(retired);
        }
        retiring.erase(settled.begin(), settled.end());
        releasedSequence = frameSequence;
    }
    released.notify_all();
}

void AndroidOutputs::receiveSurface(std::uint32_t number, ANativeWindow* window, int width, int height, float scale) {
    std::unique_lock lock(mutex);
    const auto found = entries.find(number);
    if (found == entries.end()) {
        if (window != nullptr) {
            ANativeWindow_release(window);
        }
        return;
    }
    Entry& entry = found->second;

    if (window == nullptr) {
        if (entry.window == nullptr) {
            return;
        }
        retire(entry);
        const std::uint64_t sequence = retiredSequence;
        Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
        if (!released.wait_for(lock, kReleaseTimeout, [sequence] { return releasedSequence >= sequence; })) {
            core::Log::warning("The output {} lost its window before a frame let go of it.", number);
        }
        return;
    }

    if (entry.window == window) {
        ANativeWindow_release(window);
    } else if (entry.window != nullptr) {
        retire(entry);
    }
    entry.window = window;
    entry.width = width;
    entry.height = height;
    entry.scale = scale;
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

void AndroidOutputs::receiveEnding(std::uint32_t number, Output::Ending ending, std::string reason) {
    const std::scoped_lock lock(mutex);
    const auto found = entries.find(number);
    if (found == entries.end() || found->second.ending) {
        return;
    }
    found->second.ending = ending;
    found->second.reason = std::move(reason);
    Player::postEvent({.type = Event::Type::OutputChanged, .output = number});
}

void AndroidOutputs::receiveTouch(std::uint32_t number, Event::Type type, std::span<const int> ids, std::span<const float> points, int changed) {
    Event event{.type = type, .output = number};
    event.touchCount = std::min(ids.size(), Event::kMaxTouchPoints);
    for (std::size_t index = 0; index < event.touchCount; ++index) {
        event.touches[index] = {.id = static_cast<std::uint64_t>(ids[index]), .position = {points[index * 2], points[index * 2 + 1]}, .changed = changed < 0 || static_cast<std::size_t>(changed) == index};
    }
    Player::postEvent(event);
}

void AndroidOutputs::retire(Entry& entry) {
    retiring.push_back({.sequence = ++retiredSequence, .window = entry.window, .drawable = std::move(entry.drawable)});
    entry.window = nullptr;
}

void AndroidOutputs::release(Retired& retired) noexcept {
    retired.drawable.reset();
    if (retired.window != nullptr) {
        ANativeWindow_release(retired.window);
        retired.window = nullptr;
    }
}

} // namespace haylen::platform
