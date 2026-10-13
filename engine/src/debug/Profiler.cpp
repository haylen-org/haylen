#include "haylen/debug/Profiler.hpp"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>

namespace haylen::debug {

Profiler::Profiler(std::size_t historySize, Clock source) : clock(source ? std::move(source) : Clock(&getSteadySeconds)), history(historySize, 0.0F), intervals(historySize, 0.0F) {
    if (historySize == 0) {
        throw std::invalid_argument("A profiler needs room for at least one frame.");
    }
}

double Profiler::getSteadySeconds() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void Profiler::beginFrame(double shownSeconds) {
    current.clear();
    open.clear();
    if (shownSeconds > 0.0) {
        intervals[nextInterval] = static_cast<float>(shownSeconds * 1000.0);
        nextInterval = (nextInterval + 1) % intervals.size();
        intervalCount = std::min(intervalCount + 1, intervals.size());
    }
    frameStart = clock();
    inFrame = true;
}

void Profiler::endFrame() {
    if (!inFrame) {
        throw std::logic_error("The \"endFrame\" call has no matching \"beginFrame\".");
    }

    // Scopes left open by an error close at the end of the frame, so one failure never skews the next frame.
    const double now = clock();
    endScopesTo(0);
    if (tracer) {
        tracer->record("frame", frameStart, now);
    }
    lastFrameMilliseconds = (now - frameStart) * 1000.0;
    history[next] = static_cast<float>(lastFrameMilliseconds);
    next = (next + 1) % history.size();
    recorded = std::min(recorded + 1, history.size());
    finished.swap(current);
    inFrame = false;
}

void Profiler::beginScope(std::string_view name) {
    const int parent = open.empty() ? -1 : open.back().sample;
    int index = -1;
    for (std::size_t sample = 0; sample < current.size(); ++sample) {
        if (current[sample].parent == parent && current[sample].name == name) {
            index = static_cast<int>(sample);
            break;
        }
    }
    if (index < 0) {
        index = static_cast<int>(current.size());
        current.push_back({.name = std::string(name), .milliseconds = 0.0, .calls = 0, .depth = static_cast<int>(open.size()), .parent = parent});
    }
    ++current[static_cast<std::size_t>(index)].calls;
    open.push_back({.sample = index, .start = clock()});
}

void Profiler::beginScriptScope(std::string_view name) {
    beginScope(name);
    open.back().script = true;
}

void Profiler::endScriptScope() {
    if (open.empty() || !open.back().script) {
        throw std::logic_error("A profiler scope was closed without being opened.");
    }
    endScope();
}

void Profiler::endScope() {
    if (open.empty()) {
        throw std::logic_error("A profiler scope was closed without being opened.");
    }
    const OpenScope scope = open.back();
    open.pop_back();
    const double end = clock();
    ProfileSample& sample = current[static_cast<std::size_t>(scope.sample)];
    sample.milliseconds += (end - scope.start) * 1000.0;
    if (tracer) {
        tracer->record(sample.name, scope.start, end);
    }
}

void Profiler::endScopesTo(std::size_t depth) noexcept {
    while (open.size() > depth) {
        endScope();
    }
}

// The ring of the history fills from its start, so its first entries are the recorded ones in some order, which a sum ignores.
double Profiler::getAverageFrameMilliseconds() const noexcept {
    if (recorded == 0) {
        return 0.0;
    }
    return std::accumulate(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(recorded), 0.0) / static_cast<double>(recorded);
}

double Profiler::getFramesPerSecond() const noexcept {
    const double total = std::accumulate(intervals.begin(), intervals.begin() + static_cast<std::ptrdiff_t>(intervalCount), 0.0);
    return total > 0.0 ? 1000.0 * static_cast<double>(intervalCount) / total : 0.0;
}

std::vector<float> Profiler::getFrameHistory() const {
    std::vector<float> frames;
    frames.reserve(recorded);
    const std::size_t first = (next + history.size() - recorded) % history.size();
    for (std::size_t index = 0; index < recorded; ++index) {
        frames.push_back(history[(first + index) % history.size()]);
    }
    return frames;
}

} // namespace haylen::debug
