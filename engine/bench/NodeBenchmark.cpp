#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/nodes/NodeTree.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/math/Color.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Times node trees on the CPU: the update of a large tree after every root moved and after a few moved, on one thread and on the job system, and the draw of a large tree with every node in view and with most of them culled.
class NodeBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-node-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("app.json", bytes(R"({"name": "Node Benchmark", "identifier": "com.haylen.nodebenchmark"})"));
        files.emplace("source/main.lua", std::vector<std::uint8_t>{});
        const auto package = std::make_shared<io::MemoryPackage>("node-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();
        const graphics::Texture texture = engine.getGraphics().createTexture(graphics::Image(16, 16, math::Color::white()));

        std::printf("Workers of the job system: %zu\n", engine.getJobs().getWorkerCount());
        std::printf("%-62s %12s %8s\n", "Scene", "Average ms", "Frames");
        for (const bool parallel : {false, true}) {
            measureUpdate(engine, texture, parallel, 1);
            measureUpdate(engine, texture, parallel, 100);
        }
        measureDraw(engine, host, texture, false);
        measureDraw(engine, host, texture, true);
        engine.stop();
        return 0;
    }

  private:
    static constexpr int kFrames = 240;
    static constexpr int kRoots = 1000;
    static constexpr int kChildren = 99;

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view text) {
        return {text.begin(), text.end()};
    }

    static void report(const std::string& name, double milliseconds) {
        std::printf("%-62s %12.3f %8d\n", name.c_str(), milliseconds, kFrames);
    }

    // A tree of `kRoots` units of a body and `kChildren` parts each, every node a sprite, spread over an area of 32000 by 32000 units.
    [[nodiscard]] static std::pair<std::shared_ptr<nodes2d::NodeTree>, std::vector<nodes2d::Node>> makeTree(const graphics::Texture& texture) {
        auto tree = nodes2d::NodeTree::create();
        std::vector<nodes2d::Node> roots;
        for (int root = 0; root < kRoots; ++root) {
            const math::Vec2 place{static_cast<float>(root % 32) * 1000.0F, static_cast<float>(root / 32) * 1000.0F};
            roots.push_back(tree->add({.position = place, .content = {.value = graphics2d::Sprite{.texture = texture}}}));
            for (int child = 0; child < kChildren; ++child) {
                (void)roots.back().add({.position = {static_cast<float>(child % 10) * 20.0F, static_cast<float>(child / 10) * 20.0F}, .rotation = 0.1F, .content = {.value = graphics2d::Sprite{.texture = texture}}});
            }
        }
        tree->update(0.0F);
        return {std::move(tree), std::move(roots)};
    }

    // Moves every `stride`-th root each frame and times the update that follows.
    static void measureUpdate(core::Engine& engine, const graphics::Texture& texture, bool parallel, int stride) {
        auto [tree, roots] = makeTree(texture);
        double total = 0.0;
        for (int frame = 0; frame < kFrames; ++frame) {
            for (std::size_t root = static_cast<std::size_t>(frame % stride); root < roots.size(); root += static_cast<std::size_t>(stride)) {
                roots[root].setRotation(static_cast<float>(frame) * 0.01F);
            }
            const auto start = std::chrono::steady_clock::now();
            tree->update(1.0F / 60.0F, 1.0F, parallel ? &engine.getJobs() : nullptr);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        }
        const std::string moved = stride == 1 ? "every root moved" : "1 root in 100 moved";
        report("Update of 100000 nodes, " + moved + ", " + (parallel ? "job system" : "one thread"), total / kFrames);
    }

    // Times the draw of the whole tree into a world canvas whose view covers every node, or a corner of 2000 by 1000 units.
    static void measureDraw(core::Engine& engine, platform::HeadlessHost& host, const graphics::Texture& texture, bool culled) {
        auto [tree, roots] = makeTree(texture);
        graphics2d::Renderer& renderer = engine.getRenderer2D();
        graphics2d::Camera camera;
        camera.anchor = graphics2d::Camera::Anchor::TopLeft;
        const math::Vec2 visible = engine.getViewport().getVisibleRect().getSize();
        const float zoom = culled ? visible.x / 2000.0F : std::min(visible.x, visible.y) / 33000.0F;
        camera.setMinZoom(0.01F);
        camera.setZoom({zoom, zoom});
        double total = 0.0;
        for (int frame = 0; frame < kFrames; ++frame) {
            renderer.beginFrame(engine.getViewport(), math::Color::black());
            renderer.beginWorld(camera);
            const auto start = std::chrono::steady_clock::now();
            tree->draw(renderer);
            total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            renderer.endFrame(host.getFrameTarget());
        }
        report("Draw of 100000 sprite nodes, " + std::to_string(tree->getDrawnCount()) + " in view", total / kFrames);
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::NodeBenchmark::run();
}
