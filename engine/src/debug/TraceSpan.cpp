#include "haylen/debug/TraceSpan.hpp"

#include <string>

namespace haylen::debug {

TraceSpan::TraceSpan(std::string_view actionName, std::string_view subjectName) noexcept : recorder(TraceRecorder::getThreadRecorder()), action(actionName), subject(subjectName) {
    if (recorder) {
        start = TraceRecorder::now();
    }
}

TraceSpan::~TraceSpan() {
    if (!recorder) {
        return;
    }
    std::string name;
    name.reserve(action.size() + 1 + subject.size());
    name.append(action).append(" ").append(subject);
    recorder->record(name, start, TraceRecorder::now());
}

} // namespace haylen::debug
