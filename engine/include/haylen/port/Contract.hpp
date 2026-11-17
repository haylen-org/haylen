#pragma once

namespace haylen::port {

// The contract between the engine and its ports: the headers of `haylen/port` and the function `haylen_port` of CMake. A port names the version it follows in the `CONTRACT` of its `haylen_port`, which the configuration of the engine compares with `kVersion`, and the version grows whenever the contract changes in a way that a port must follow.
class Contract final {
  public:
    static constexpr int kVersion = 2;
};

} // namespace haylen::port
