#include <any>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/debug/Profiler.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"

namespace haylen::bench {

// Draws each phase for a while with vsync off and reports the wall time between frames, which includes the GPU and displays that pace frames themselves, like macOS does. The work column is the CPU time of the frame outside the submit step, where the renderer uploads and waits for the next drawable. Dynamic phases move every sprite every frame, and baked phases draw a static batch.
class SpriteBenchmark final : public core::Application {
  public:
    void start(core::Engine& engine) override {
        engine.getScenes().push(std::make_shared<Stage>());
    }

  private:
    struct Phase {
        std::size_t sprites = 0;
        bool baked = false;
    };

    class Stage final : public core::Scene {
      public:
        void enter(core::Engine& engine, const std::any&) override {
            texture = engine.getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
            core::Log::info("{:>10} {:<8} {:>9} {:>9} {:>14} {:>9}", "sprites", "kind", "ms", "fps", "Msprites/s", "work ms");
            begin(engine);
        }

        void update(core::Engine& engine, float) override {
            const auto now = std::chrono::steady_clock::now();
            if (frame == kWarmupFrames) {
                measureStart = now;
                workMilliseconds = 0.0;
            } else if (frame > kWarmupFrames) {
                workMilliseconds += getWorkMilliseconds(engine.getProfiler());
            }
            if (frame == kWarmupFrames + kMeasuredFrames) {
                finishPhase(engine, now);
                return;
            }
            ++frame;

            elapsed += 1.0F / 60.0F;
            if (!kPhases[phaseIndex].baked) {
                animate(engine);
            }
        }

        void render(core::Engine& engine) override {
            graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(camera);
            if (kPhases[phaseIndex].baked) {
                renderer.drawStatic(baked, {}, {std::sin(elapsed) * 8.0F, 0.0F});
            } else {
                renderer.drawBatch(texture, sprites);
            }
        }

      private:
        static constexpr std::array<Phase, 6> kPhases{{{100'000, false}, {100'000, true}, {1'000'000, false}, {1'000'000, true}, {2'000'000, false}, {2'000'000, true}}};
        static constexpr int kWarmupFrames = 30;
        static constexpr int kMeasuredFrames = 240;
        static constexpr std::size_t kParallelGrain = 16384;

        [[nodiscard]] static double getWorkMilliseconds(const debug::Profiler& profiler) {
            double total = 0.0;
            for (const debug::ProfileSample& sample : profiler.getLastFrame()) {
                if (sample.depth == 0 && sample.name != "submit") {
                    total += sample.milliseconds;
                }
            }
            return total;
        }

        void begin(core::Engine& engine) {
            const Phase& current = kPhases[phaseIndex];
            frame = 0;
            camera.position = {960.0F, 540.0F};
            origins.resize(current.sprites);
            sprites.resize(current.sprites);

            // Sprites cover the design area in a grid dense enough to hold every sprite of the phase.
            const auto columns = static_cast<std::size_t>(std::ceil(std::sqrt(static_cast<double>(current.sprites) * 1920.0 / 1080.0)));
            const float spacing = 1920.0F / static_cast<float>(columns);
            for (std::size_t index = 0; index < current.sprites; ++index) {
                origins[index] = {(static_cast<float>(index % columns) + 0.5F) * spacing, (static_cast<float>(index / columns) + 0.5F) * spacing};
                const float hue = static_cast<float>(index % 360) / 360.0F;
                sprites[index] = {.position = origins[index], .size = {3.0F, 3.0F}, .color = {0.4F + hue * 0.6F, 0.8F - hue * 0.5F, 1.0F - hue * 0.3F, 1.0F}};
            }
            baked = current.baked ? engine.getRenderer2D().createStaticBatch(texture, sprites) : graphics2d::StaticSpriteBatch{};
        }

        void animate(core::Engine& engine) {
            const float seconds = elapsed;
            // clang-format off
            engine.getJobs().parallelFor(0, sprites.size(), kParallelGrain, [this, seconds](std::size_t first, std::size_t last) {
                for (std::size_t index = first; index < last; ++index) {
                    const float offset = static_cast<float>(index % 1024) * 0.006F;
                    sprites[index].position = origins[index] + math::Vec2{std::sin(seconds * 2.0F + offset), std::cos(seconds * 1.5F + offset)} * 4.0F;
                }
            });
            // clang-format on
        }

        void finishPhase(core::Engine& engine, std::chrono::steady_clock::time_point now) {
            const Phase& current = kPhases[phaseIndex];
            const double milliseconds = std::chrono::duration<double, std::milli>(now - measureStart).count() / kMeasuredFrames;
            const double fps = 1000.0 / milliseconds;
            const double work = workMilliseconds / kMeasuredFrames;
            core::Log::info("{:>10} {:<8} {:>9.2f} {:>9.1f} {:>14.1f} {:>9.2f}", current.sprites, current.baked ? "baked" : "dynamic", milliseconds, fps, static_cast<double>(current.sprites) * fps / 1'000'000.0, work);

            if (++phaseIndex == kPhases.size()) {
                engine.quit();
                return;
            }
            begin(engine);
        }

        graphics::Texture texture;
        graphics2d::Camera camera;
        std::vector<math::Vec2> origins;
        std::vector<graphics2d::SpriteInstance> sprites;
        graphics2d::StaticSpriteBatch baked;
        std::size_t phaseIndex = 0;
        int frame = 0;
        float elapsed = 0.0F;
        double workMilliseconds = 0.0;
        std::chrono::steady_clock::time_point measureStart;
    };
};

} // namespace haylen::bench

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<haylen::bench::SpriteBenchmark>();
}
