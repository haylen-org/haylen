#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "support/EngineFixture.hpp"
#include "support/RecordingScene.hpp"
#include "support/TestApplication.hpp"

namespace haylen::core {

// The plugins update before the scenes and update late after them, in the same frame.
class FrameOrderTest : public ::testing::Test {
  protected:
    // Writes the updates that the scene ran when each hook of the plugin runs.
    class Watcher final : public plugins::Plugin {
      public:
        Watcher(const test::RecordingScene& watched, std::vector<std::string>& entries) : scene(watched), log(entries) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return "watcher";
        }
        void update(Engine&, float) override {
            log.push_back("update " + std::to_string(scene.updates));
        }
        void lateUpdate(Engine&, float) override {
            log.push_back("lateUpdate " + std::to_string(scene.updates));
        }
        void render(Engine&) override {
            log.push_back("render");
        }

      private:
        const test::RecordingScene& scene;
        std::vector<std::string>& log;
    };

    std::vector<std::string> sceneLog;
    std::vector<std::string> log;
    std::shared_ptr<test::RecordingScene> scene = std::make_shared<test::RecordingScene>("main", sceneLog);
    // clang-format off
    test::EngineFixture app{{}, std::make_unique<test::TestApplication>([this](Engine& engine) {
        engine.addPlugin(std::make_unique<Watcher>(*scene, log));
        engine.getScenes().push(scene);
    })};
    // clang-format on
};

TEST_F(FrameOrderTest, RunsTheLateUpdateOfPluginsAfterTheScenesUpdated) {
    app.frames(2);
    EXPECT_EQ(log, (std::vector<std::string>{"update 0", "lateUpdate 1", "render", "update 1", "lateUpdate 2", "render"}));
}

} // namespace haylen::core
