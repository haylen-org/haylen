#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/animation/AtlasBuilder.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Scene.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/plugins/TextPlugin.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/RichText.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Placement.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Times whole engine frames that draw sprites, shapes, text, rich text and a screen of interface components on the headless host, and prints for each scene the CPU time of a frame with the draw calls, instances and uploaded bytes the renderer reports, and the allocations of the frame thread.
class RenderBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-render-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        files.emplace("app.json", bytes(R"({"name": "Render Benchmark", "identifier": "dev.haylen.render-benchmark", "design": {"width": 1920, "height": 1080}})"));
        files.emplace("source/main.lua", std::vector<std::uint8_t>{});
        const auto package = std::make_shared<io::MemoryPackage>("render-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();

        std::printf("%-44s %9s %7s %9s %11s %9s %11s\n", "Scene", "Frame ms", "Draws", "Instances", "Upload KB", "Allocs", "Alloc KB");
        measureSprites(engine);
        measureShapes(engine);
        measureText(engine);
        measureRichText(engine);
        measureInterface(engine);
        measurePacking();
        const bool failed = engine.getError() != nullptr;
        engine.stop();
        return failed ? 1 : 0;
    }

    // Counts the allocations of the frame thread while a measurement runs.
    class Allocations final {
      public:
        static void record(std::size_t size) noexcept {
            if (counting) {
                ++count;
                total += size;
            }
        }

        static thread_local bool counting;
        static thread_local std::uint64_t count;
        static thread_local std::uint64_t total;
    };

  private:
    using Draw = std::function<void(core::Engine&)>;

    // Runs the drawing of a measurement inside a real frame.
    class DrawingScene final : public core::Scene {
      public:
        explicit DrawingScene(Draw function) : draw(std::move(function)) {}

        void render(core::Engine& engine) override {
            draw(engine);
        }

      private:
        Draw draw;
    };

    static constexpr int kWarmupFrames = 30;
    static constexpr int kFrames = 300;
    static constexpr double kFrameSeconds = 1.0 / 60.0;

    [[nodiscard]] static std::vector<std::uint8_t> bytes(std::string_view text) {
        return {text.begin(), text.end()};
    }

    static void measure(core::Engine& engine, std::string_view name, Draw draw) {
        engine.getScenes().replace(std::make_shared<DrawingScene>(std::move(draw)));
        for (int frame = 0; frame < kWarmupFrames; ++frame) {
            engine.frame(kFrameSeconds);
        }

        double milliseconds = 0.0;
        std::uint64_t uploaded = 0;
        Allocations::count = 0;
        Allocations::total = 0;
        for (int frame = 0; frame < kFrames; ++frame) {
            const auto started = std::chrono::steady_clock::now();
            Allocations::counting = true;
            engine.frame(kFrameSeconds);
            Allocations::counting = false;
            milliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            uploaded += engine.getRenderer2D().getStats().uploadedBytes;
        }
        const graphics2d::Renderer::Stats stats = engine.getRenderer2D().getStats();
        std::printf("%-44.*s %9.3f %7zu %9zu %11.1f %9.1f %11.2f\n", static_cast<int>(name.size()), name.data(), milliseconds / kFrames, stats.drawCalls, stats.instances, static_cast<double>(uploaded) / kFrames / 1024.0, static_cast<double>(Allocations::count) / kFrames, static_cast<double>(Allocations::total) / kFrames / 1024.0);
    }

    // Ten thousand sprites in runs of one texture each, as an atlas per layer draws them, then the same sprites alternating textures, which breaks every batch.
    static void measureSprites(core::Engine& engine) {
        std::array<graphics::Texture, 4> textures;
        for (std::size_t index = 0; index < textures.size(); ++index) {
            textures[index] = engine.getGraphics().createTexture(graphics::Image(32, 32, math::Color{0.25F * static_cast<float>(index + 1), 0.5F, 0.8F, 1.0F}));
        }
        const graphics2d::Camera camera;
        for (const bool alternate : {false, true}) {
            // clang-format off
            measure(engine, alternate ? "Sprites, 10000 alternating 4 textures" : "Sprites, 10000 in runs of 4 textures", [&textures, &camera, alternate](core::Engine& current) {
                graphics2d::Renderer& renderer = current.getRenderer2D();
                renderer.beginWorld(camera);
                for (int index = 0; index < 10000; ++index) {
                    const std::size_t texture = alternate ? static_cast<std::size_t>(index) % textures.size() : static_cast<std::size_t>(index) / 2500;
                    renderer.draw({.texture = textures[texture], .position = {static_cast<float>(index % 100) * 19.0F - 950.0F, static_cast<float>(index / 100) * 10.0F - 500.0F}, .size = {16.0F, 16.0F}, .rotation = static_cast<float>(index) * 0.01F});
                }
            });
            // clang-format on
        }

        // The same alternating sprites cut from one atlas that packs the four images at run time.
        animation2d::AtlasBuilder builder;
        for (std::size_t index = 0; index < textures.size(); ++index) {
            builder.add(std::to_string(index), std::make_shared<graphics::Image>(32, 32, math::Color{0.25F * static_cast<float>(index + 1), 0.5F, 0.8F, 1.0F}));
        }
        animation2d::AtlasBuilder::Packed packed = builder.pack({});
        graphics::Texture image = engine.getGraphics().createTexture(packed.image);
        const animation2d::SpriteAtlas atlas = animation2d::AtlasBuilder::finish(std::move(packed), image);
        // clang-format off
        measure(engine, "Sprites, 10000 alternating 4 atlas frames", [&atlas, &camera](core::Engine& current) {
            graphics2d::Renderer& renderer = current.getRenderer2D();
            renderer.beginWorld(camera);
            for (int index = 0; index < 10000; ++index) {
                const math::Rect source = atlas.getFrame(atlas.getFrameNames()[static_cast<std::size_t>(index) % 4]).source;
                renderer.draw({.texture = atlas.getTexture(), .source = source, .position = {static_cast<float>(index % 100) * 19.0F - 950.0F, static_cast<float>(index / 100) * 10.0F - 500.0F}, .size = {16.0F, 16.0F}, .rotation = static_cast<float>(index) * 0.01F});
            }
        });
        // clang-format on
    }

    // Packs 400 images of 8 to 72 pixels a side, trimmed, padded and extruded, as a game packs its art when it starts.
    static void measurePacking() {
        animation2d::AtlasBuilder builder;
        for (int index = 0; index < 400; ++index) {
            const int width = 8 + (index * 37) % 65;
            const int height = 8 + (index * 53) % 65;
            auto image = std::make_shared<graphics::Image>(width, height);
            image->fill({1, 1, width - 2, height - 2}, math::Color::white());
            builder.add(std::to_string(index), std::move(image));
        }
        const auto start = std::chrono::steady_clock::now();
        const animation2d::AtlasBuilder::Packed packed = builder.pack({.texture = {.filter = graphics::Texture::Filter::Linear}});
        const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        std::printf("Packing 400 images into a linear atlas of %d by %d pixels: %.2f ms\n", packed.image.getWidth(), packed.image.getHeight(), milliseconds);
    }

    static void measureShapes(core::Engine& engine) {
        // clang-format off
        measure(engine, "Shapes, 3000 circles, panels and lines", [](core::Engine& current) {
            graphics2d::Renderer& renderer = current.getRenderer2D();
            renderer.beginScreen();
            for (int index = 0; index < 1000; ++index) {
                const math::Vec2 place{static_cast<float>(index % 40) * 48.0F, static_cast<float>(index / 40) * 42.0F};
                renderer.drawCircle(place + math::Vec2{12.0F, 12.0F}, 10.0F, math::Color{0.9F, 0.4F, 0.2F, 1.0F});
                renderer.drawShape({.bounds = {place.x + 24.0F, place.y, 20.0F, 20.0F}, .radii = {4.0F, 4.0F, 4.0F, 4.0F}, .color = math::Color{0.2F, 0.3F, 0.5F, 1.0F}, .borderWidth = 1.0F, .borderColor = math::Color::white()});
                renderer.drawLine(place + math::Vec2{0.0F, 30.0F}, place + math::Vec2{44.0F, 36.0F}, 2.0F, math::Color{0.6F, 0.9F, 0.6F, 1.0F});
            }
        });
        // clang-format on
    }

    // Labels that stay find their layouts in the cache of the font, and labels whose numbers change every frame lay out every frame.
    static void measureText(core::Engine& engine) {
        std::vector<std::string> labels;
        for (int index = 0; index < 200; ++index) {
            labels.push_back(std::format("Label number {} of the screen", index + 1));
        }
        // clang-format off
        measure(engine, "Text, 200 labels that stay", [&labels](core::Engine& current) {
            graphics2d::Renderer& renderer = current.getRenderer2D();
            renderer.beginScreen();
            for (std::size_t index = 0; index < labels.size(); ++index) {
                renderer.drawText(*current.getDefaultFont(), labels[index], {static_cast<float>(index % 5) * 380.0F, static_cast<float>(index / 5) * 26.0F}, {.size = 20.0F});
            }
        });
        std::string changing;
        measure(engine, "Text, 200 labels that change every frame", [&changing](core::Engine& current) {
            graphics2d::Renderer& renderer = current.getRenderer2D();
            renderer.beginScreen();
            for (int index = 0; index < 200; ++index) {
                changing.clear();
                std::format_to(std::back_inserter(changing), "Score {} of player {}", current.getClock().getFrameIndex() * 7 + static_cast<std::uint64_t>(index), index);
                renderer.drawText(*current.getDefaultFont(), changing, {static_cast<float>(index % 5) * 380.0F, static_cast<float>(index / 5) * 26.0F}, {.size = 20.0F});
            }
        });
        // clang-format on
    }

    // Blocks of markup with colors, styles and an effect that moves every character every frame.
    static void measureRichText(core::Engine& engine) {
        const std::shared_ptr<text::RichTextRegistry> registry = engine.getPlugin<plugins::TextPlugin>().getRegistry();
        std::vector<std::unique_ptr<text::RichText>> blocks;
        for (int index = 0; index < 20; ++index) {
            blocks.push_back(std::make_unique<text::RichText>("A [b]bold[/b] word, a [color=#ff8040]colored[/color] one, [i]italics[/i] and a [wave]wave that moves[/wave] in a block that wraps its lines.", text::RichTextOptions{.family = registry->getDefaultFamily(), .size = 22.0F, .maxWidth = 420.0F}, registry));
        }
        // clang-format off
        measure(engine, "Rich text, 20 blocks with a wave", [&blocks](core::Engine& current) {
            graphics2d::Renderer& renderer = current.getRenderer2D();
            renderer.beginScreen();
            for (std::size_t index = 0; index < blocks.size(); ++index) {
                blocks[index]->update(static_cast<float>(kFrameSeconds));
                renderer.drawRichText(*blocks[index], {static_cast<float>(index % 4) * 460.0F, static_cast<float>(index / 4) * 200.0F});
            }
        });
        // clang-format on
    }

    // A settings screen with a component of every common kind.
    static void measureInterface(core::Engine& engine) {
        auto& plugin = engine.getPlugin<plugins::UiPlugin>();
        // clang-format off
        const std::shared_ptr<ui::Gui> gui = plugin.createGui(core::Json::parse(R"({"kind": "scroll", "children": [{"kind": "column", "padding": 20, "gap": 8, "children": [
            {"kind": "pageHeader", "title": "Settings of the whole app", "caption": "Everything that the app lets you change"},
            {"kind": "label", "text": "A label that wraps its text over the lines it needs to show it all"},
            {"kind": "row", "gap": 8, "children": [{"kind": "button", "text": "Accept everything"}, {"kind": "button", "text": "Cancel"}, {"kind": "chip", "text": "Filter chip"}, {"kind": "badge", "text": "12"}]},
            {"kind": "panel", "children": [{"kind": "slider", "value": 0.4}, {"kind": "rangeSlider", "low": 0.2, "high": 0.6}, {"kind": "progress", "value": 0.3, "text": "Downloading the new levels"}]},
            {"kind": "toggle", "text": "Music and sound effects", "checked": true}, {"kind": "checkbox", "text": "Show the hints of every level", "checked": true},
            {"kind": "textField", "value": "Typed text in the field", "placeholder": "Name"}, {"kind": "numberField", "value": 42}, {"kind": "stepper", "value": 3},
            {"kind": "combo", "items": [{"id": "a", "text": "Alpha channel"}, {"id": "b", "text": "Beta"}], "selected": "a"},
            {"kind": "segmentedControl", "items": [{"id": "a", "text": "Alpha"}, {"id": "b", "text": "Beta"}, {"id": "c", "text": "Gamma"}], "selected": "a"},
            {"kind": "radioGroup", "items": [{"id": "a", "text": "Alpha channel only"}, {"id": "b", "text": "Beta"}], "selected": "a"},
            {"kind": "circularProgress", "value": 0.6, "text": "60%"},
            {"kind": "tabs", "items": [{"id": "a", "text": "General settings"}, {"id": "b", "text": "Audio"}], "selected": "a", "children": [{"kind": "label", "text": "Tab one"}, {"kind": "label", "text": "Tab two"}]},
            {"kind": "list", "items": [{"id": "a", "text": "First row of the list"}, {"id": "b", "text": "Second row of the list"}, {"id": "c", "text": "Third row of the list"}]},
            {"kind": "card", "children": [{"kind": "richText", "text": "Some [b]bold[/b] and [color=#ff0000]red[/color] text inside a card"}]},
            {"kind": "settingsRow", "label": "Volume of the music", "caption": "Applies to every scene", "children": [{"kind": "slider", "value": 0.5}]},
            {"kind": "alert", "title": "Heads up", "message": "Something needs your attention right now"},
            {"kind": "table", "columns": [{"text": "Name of the player"}, {"text": "Score"}], "rows": [{"id": "one", "cells": ["Player number one", "120"]}, {"id": "two", "cells": ["Player number two", "95"]}]},
            {"kind": "divider"}, {"kind": "statusIndicator", "text": "Online and ready", "tone": "success"}, {"kind": "avatar", "name": "Ana Lima"}
        ]}]})"), ui::Placement::Screen);
        // clang-format on
        plugin.mount(gui);
        measure(engine, "Interface, a screen of 40 components", [](core::Engine&) {});
        plugin.unmount(*gui);
    }
};

thread_local bool RenderBenchmark::Allocations::counting = false;
thread_local std::uint64_t RenderBenchmark::Allocations::count = 0;
thread_local std::uint64_t RenderBenchmark::Allocations::total = 0;

} // namespace haylen::bench

// The standard names the replaceable allocation functions, so the benchmark replaces them here, and they only forward to the counter and to `malloc` and `free`. The array and nothrow forms call these by default.
void* operator new(std::size_t size) {
    haylen::bench::RenderBenchmark::Allocations::record(size);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

int main() {
    return haylen::bench::RenderBenchmark::run();
}
