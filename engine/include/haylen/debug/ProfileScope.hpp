#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/debug/Profiler.hpp"

namespace haylen::debug {

// Opens a profiler scope for as long as it lives. Scopes that code inside it left open close with it.
class ProfileScope final {
  public:
    ProfileScope(Profiler& target, std::string_view name) : profiler(target), depth(target.getDepth()) {
        profiler.beginScope(name);
    }
    ~ProfileScope() {
        profiler.endScopesTo(depth);
    }

    ProfileScope(const ProfileScope&) = delete;
    ProfileScope& operator=(const ProfileScope&) = delete;

  private:
    Profiler& profiler;
    std::size_t depth;
};

} // namespace haylen::debug
