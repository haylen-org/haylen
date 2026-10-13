#include "haylen/debug/TraceRecorder.hpp"

#include <algorithm>
#include <chrono>
#include <format>

#include "haylen/core/Json.hpp"

namespace haylen::debug {

TraceRecorder::TraceRecorder(std::size_t spanCount) : laneNames{"gpu"}, capacity(spanCount), origin(now()) {
    threads.emplace_back();
    spans.reserve(std::min<std::size_t>(spanCount, 4096));
}

double TraceRecorder::now() noexcept {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

void TraceRecorder::record(std::string_view name, double startSeconds, double endSeconds) {
    const std::scoped_lock lock(mutex);
    append(name, laneOfThisThread(), startSeconds, endSeconds - startSeconds);
}

void TraceRecorder::recordGpu(std::string_view name, double endSeconds, double durationSeconds) {
    const std::scoped_lock lock(mutex);
    append(name, kGpuLane, endSeconds - durationSeconds, durationSeconds);
}

void TraceRecorder::nameThread(std::string_view name) {
    const std::scoped_lock lock(mutex);
    laneNames[laneOfThisThread()] = std::string(name);
}

void TraceRecorder::append(std::string_view name, std::uint32_t lane, double startSeconds, double durationSeconds) {
    if (spans.size() >= capacity) {
        ++dropped;
        return;
    }
    spans.push_back({.name = intern(name), .lane = lane, .start = (startSeconds - origin) * 1e6, .duration = std::max(durationSeconds, 0.0) * 1e6});
}

std::uint32_t TraceRecorder::intern(std::string_view name) {
    if (const auto found = nameIndices.find(name); found != nameIndices.end()) {
        return found->second;
    }
    const auto index = static_cast<std::uint32_t>(names.size());
    names.emplace_back(name);
    nameIndices.emplace(names.back(), index);
    return index;
}

std::uint32_t TraceRecorder::laneOfThisThread() {
    const std::thread::id self = std::this_thread::get_id();
    const auto found = std::ranges::find(threads, self);
    if (found != threads.end()) {
        return static_cast<std::uint32_t>(found - threads.begin());
    }
    threads.push_back(self);
    laneNames.push_back(std::format("thread {}", threads.size() - 1));
    return static_cast<std::uint32_t>(threads.size() - 1);
}

std::size_t TraceRecorder::size() const {
    const std::scoped_lock lock(mutex);
    return spans.size();
}

std::size_t TraceRecorder::getDropped() const {
    const std::scoped_lock lock(mutex);
    return dropped;
}

// The text is written directly, since a full trace holds hundreds of thousands of spans, and only the names go through the JSON escaping.
std::string TraceRecorder::toJson() const {
    const std::scoped_lock lock(mutex);
    std::vector<std::string> quoted;
    quoted.reserve(names.size());
    for (const std::string& name : names) {
        quoted.push_back(core::Json(name).dump());
    }

    std::string text = "{\"traceEvents\":[";
    text.reserve(spans.size() * 80 + laneNames.size() * 160);
    for (std::size_t lane = 0; lane < laneNames.size(); ++lane) {
        const std::size_t order = lane == kGpuLane ? laneNames.size() : lane;
        text += std::format(R"({}{{"name":"thread_name","ph":"M","pid":1,"tid":{},"args":{{"name":{}}}}},{{"name":"thread_sort_index","ph":"M","pid":1,"tid":{},"args":{{"sort_index":{}}}}})", lane == 0 ? "" : ",", lane, core::Json(laneNames[lane]).dump(), lane, order);
    }
    for (const Span& span : spans) {
        text += std::format(R"(,{{"name":{},"ph":"X","pid":1,"tid":{},"ts":{:.3f},"dur":{:.3f}}})", quoted[span.name], span.lane, span.start, span.duration);
    }
    text += std::format(R"(],"displayTimeUnit":"ms","otherData":{{"dropped":{}}}}})", dropped);
    return text;
}

} // namespace haylen::debug
