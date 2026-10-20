#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/assets/Manager.hpp"
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

// Draws each phase for a while with vsync off and reports the wall time between frames, which includes the GPU and displays that pace frames themselves, like macOS does, and the slowest frame. The work column is the CPU time of the frame outside the submit step, where the renderer uploads and waits for the next drawable. Dynamic phases move every sprite every frame, and baked phases draw a static batch. The last phase loads textures in the background while it draws, and lasts until they are ready, so its frames show what finishing them on the frame thread within the upload budget of the asset manager costs.
class SpriteBenchmark final : public core::Application {
  public:
    void start(core::Engine& engine) override {
        registerNoise(engine);
        engine.getScenes().push(std::make_shared<Stage>());
    }

  private:
    struct Phase {
        std::size_t sprites = 0;
        bool baked = false;
        int textures = 0;
    };

    // Images of noise that `content/noise.json` describes, decoded on a worker and made into a texture on the frame thread like a picture of the app, while their pixels differ by the seed of each load.
    static void registerNoise(core::Engine& engine) {
        graphics::Device& device = engine.getGraphics();
        // clang-format off
        engine.getAssets().registerType({
            .name = "noise",
            .normalize = [](const core::Json& options) { return core::Json{{"seed", options.at("seed").get<std::uint64_t>()}}; },
            .decode = [](assets::Manager::Request& request) {
                const int size = core::Json::parse(request.bytes).at("size").get<int>();
                std::vector<std::uint8_t> pixels(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4);
                std::uint64_t state = request.options.at("seed").get<std::uint64_t>();
                for (std::uint8_t& value : pixels) {
                    state = state * 6364136223846793005ULL + 1442695040888963407ULL;
                    value = static_cast<std::uint8_t>(state >> 56);
                }
                return std::make_shared<graphics::Image>(size, size, std::move(pixels));
            },
            .finalize = [&device](std::shared_ptr<void> decoded, const assets::Manager::Request&) {
                return std::make_shared<graphics::Texture>(device.createTexture(*std::static_pointer_cast<graphics::Image>(decoded)));
            },
        });
        // clang-format on
    }

    class Stage final : public core::Scene {
      public:
        void enter(core::Engine& engine, const std::any&) override {
            texture = engine.getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));
            core::Log::info("{:>10} {:<22} {:>9} {:>9} {:>9} {:>14} {:>9} {:>7}", "sprites", "kind", "ms", "max ms", "fps", "Msprites/s", "work ms", "frames");
            begin(engine);
        }

        void update(core::Engine& engine, float) override {
            const auto now = std::chrono::steady_clock::now();
            const Phase& current = kPhases[phaseIndex];
            if (frame == kWarmupFrames) {
                measureStart = now;
                workMilliseconds = 0.0;
                slowestMilliseconds = 0.0;
                loadTextures(engine, current.textures);
            } else if (frame > kWarmupFrames) {
                workMilliseconds += getWorkMilliseconds(engine.getProfiler());
                slowestMilliseconds = std::max(slowestMilliseconds, std::chrono::duration<double, std::milli>(now - previousFrame).count());
            }
            previousFrame = now;
            const bool measured = current.textures > 0 ? frame > kWarmupFrames && loading == 0 : frame == kWarmupFrames + kMeasuredFrames;
            if (measured) {
                finishPhase(engine, now, frame - kWarmupFrames);
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
        static constexpr std::array<Phase, 7> kPhases{{{100'000, false, 0}, {100'000, true, 0}, {1'000'000, false, 0}, {1'000'000, true, 0}, {2'000'000, false, 0}, {2'000'000, true, 0}, {100'000, false, 48}}};
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

        void loadTextures(core::Engine& engine, int count) {
            loading = count;
            textures.clear();
            for (int index = 0; index < count; ++index) {
                // clang-format off
                engine.getAssets().loadAsync("noise", "noise.json", [this](std::shared_ptr<void> asset, const std::string&) {
                    textures.push_back(std::move(asset));
                    --loading;
                }, core::Json{{"seed", index + 1}});
                // clang-format on
            }
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

        void finishPhase(core::Engine& engine, std::chrono::steady_clock::time_point now, int frames) {
            const Phase& current = kPhases[phaseIndex];
            const double milliseconds = std::chrono::duration<double, std::milli>(now - measureStart).count() / frames;
            const double fps = 1000.0 / milliseconds;
            const double work = workMilliseconds / frames;
            const std::string kind = current.textures > 0 ? std::format("dynamic, {} textures", current.textures) : current.baked ? "baked" : "dynamic";
            core::Log::info("{:>10} {:<22} {:>9.2f} {:>9.2f} {:>9.1f} {:>14.1f} {:>9.2f} {:>7}", current.sprites, kind, milliseconds, slowestMilliseconds, fps, static_cast<double>(current.sprites) * fps / 1'000'000.0, work, frames);
            textures.clear();

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
        double slowestMilliseconds = 0.0;
        std::chrono::steady_clock::time_point measureStart;
        std::chrono::steady_clock::time_point previousFrame;
        std::vector<std::shared_ptr<void>> textures;
        int loading = 0;
    };
};

} // namespace haylen::bench

std::unique_ptr<haylen::core::Application> haylen::core::Application::create() {
    return std::make_unique<haylen::bench::SpriteBenchmark>();
}
