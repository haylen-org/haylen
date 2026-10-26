#pragma once

#include <functional>
#include <type_traits>
#include <vector>

#include "gpu/opengl/Gl.hpp"

namespace haylen::gpu::opengl {

// Counts the calls that reach the entry points it watches in a loaded function table, by putting a counting function in front of each of them, and puts the entry points back when it ends.
class GlCallCounter final {
  public:
    explicit GlCallCounter(Gl& functions) noexcept : gl(functions) {
        calls = 0;
    }

    GlCallCounter(const GlCallCounter&) = delete;
    GlCallCounter& operator=(const GlCallCounter&) = delete;

    ~GlCallCounter() {
        for (const std::function<void()>& restore : restorers) {
            restore();
        }
    }

    template <auto Member> void watch() {
        using Entry = Counted<Member, std::remove_reference_t<decltype(gl.*Member)>>;
        Entry::original = gl.*Member;
        gl.*Member = &Entry::forward;
        restorers.emplace_back([this] { gl.*Member = Entry::original; });
    }

    [[nodiscard]] int getCalls() const noexcept {
        return calls;
    }

    void reset() noexcept {
        calls = 0;
    }

  private:
    template <auto Member, typename Function> struct Counted;

    template <auto Member, typename Result, typename... Arguments> struct Counted<Member, Result (*)(Arguments...)> {
        static inline Result (*original)(Arguments...) = nullptr;

        static Result forward(Arguments... arguments) {
            ++calls;
            return original(arguments...);
        }
    };

    static inline int calls = 0;
    Gl& gl;
    std::vector<std::function<void()>> restorers;
};

} // namespace haylen::gpu::opengl
