#include "support/EventLoop.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <chrono>
#include <thread>
#endif

namespace haylen::test {

void EventLoop::yield() {
#if defined(__EMSCRIPTEN__)
    emscripten_sleep(1);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
#endif
}

} // namespace haylen::test
