#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/debug/Profiler.hpp"

namespace haylen::debug {

// Opens a profiler scope for as long as it lives. Scopes that code inside it left open close with it.
class ProfileScope final {
  public:
    // A scope of the engine names the step that runs, such as `update`, while a scope that app code asks for only measures.
    enum class Owner { Engine, Script };

    ProfileScope(Profiler& target, std::string_view name, Owner owner = Owner::Engine) : profiler(target), depth(target.getDepth()) {
        if (owner == Owner::Script) {
            profiler.beginScriptScope(name);
            return;
        }

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
