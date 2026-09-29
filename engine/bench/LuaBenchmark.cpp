#include <cstdio>
#include <filesystem>
#include <memory>
#include <utility>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Runs the Lua bunnymark package on the headless host, where no GPU paces the frames, so the numbers it prints are the CPU cost of Lua updating and drawing many sprites through tables, a float buffer and a sprite batch.
class LuaBenchmark final {
  public:
    static int run(const std::filesystem::path& folder) {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-lua-benchmark");
        const std::shared_ptr<io::Package> package = io::Package::open(folder);
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);

        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();
        while (engine.isRunning() && engine.getError() == nullptr) {
            engine.frame(1.0 / 60.0);
        }
        const bool failed = engine.getError() != nullptr;
        engine.stop();
        return failed ? 1 : 0;
    }
};

} // namespace haylen::bench

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "Usage: haylen-lua-benchmark <package folder>\n");
        return 2;
    }
    return haylen::bench::LuaBenchmark::run(argv[1]);
}
