#pragma once

namespace haylen::test {

// The turn that a test that waits for the GPU gives the event loop of its platform. The browser settles the promises of WebGPU, such as the mapping of a readback or the adapter of a device, only in a turn of its event loop, which the tests of the web reach through the promise integration of WebAssembly, and the other platforms take it as a short pause.
class EventLoop final {
  public:
    EventLoop() = delete;

    static void yield();
};

} // namespace haylen::test
