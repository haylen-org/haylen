#pragma once

#include <string_view>

#include "haylen/debug/TraceRecorder.hpp"

namespace haylen::debug {

// Records a span named after what the calling thread does and what it does it to, such as `decode sprites/hero.png`, into the recorder of the thread for as long as it lives. A thread without a recorder records nothing, and the span then costs one read of a thread-local value. Both texts must outlive the span.
class TraceSpan final {
  public:
    TraceSpan(std::string_view action, std::string_view subject) noexcept;
    ~TraceSpan();

    TraceSpan(const TraceSpan&) = delete;
    TraceSpan& operator=(const TraceSpan&) = delete;

  private:
    TraceRecorder* recorder;
    std::string_view action;
    std::string_view subject;
    double start = 0.0;
};

} // namespace haylen::debug
