#pragma once

namespace haylen::platform {

// Starts the web runtime in the mode of its page: on the page, where the platform layer of SDL reads the document, or in a dedicated worker, the Worker mode, whose page posts what it reads, as the page names it with `Module.haylen.hosting`.
class WebMain final {
  public:
    [[nodiscard]] static int run(int argc, char* argv[]);
};

} // namespace haylen::platform
