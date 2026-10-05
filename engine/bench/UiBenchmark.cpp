#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Collection.hpp"
#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/CollectionItems.hpp"
#include "haylen/ui/Gui.hpp"
#include "platform/headless/HeadlessHost.hpp"

namespace haylen::bench {

// Flings collections of a hundred thousand items of three types on the headless host, in a list and in a grid, and prints the CPU time of a frame with the items bound, the binder calls and the cells created, then the same counts for frames at rest.
class UiBenchmark final {
  public:
    static int run() {
        platform::HeadlessHost host(std::filesystem::temp_directory_path() / "haylen-ui-benchmark");
        std::map<std::string, std::vector<std::uint8_t>> files;
        for (const auto& [path, text] : {std::pair<std::string, std::string_view>{"app.json", R"({"name": "UI Benchmark", "identifier": "dev.haylen.uibenchmark"})"}, {"source/main.lua", ""}}) {
            files.emplace(path, std::vector<std::uint8_t>(text.begin(), text.end()));
        }
        const auto package = std::make_shared<io::MemoryPackage>("ui-benchmark", std::move(files));
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        core::Engine engine(host, package, std::move(config), std::move(application));
        engine.start();

        std::printf("%-6s %-6s %7s %9s %9s %8s %10s %8s %7s\n", "layout", "phase", "frames", "frame ms", "worst ms", "binds", "binder", "created", "alive");
        measure(engine, "list", kList, [](std::size_t index) { return index % 50 == 0 ? "header" : index % 3 == 0 ? "detail" : "plain"; }, "detail");
        measure(engine, "grid", kGrid, [](std::size_t index) { return index % 40 == 0 ? "header" : index % 7 == 0 ? "wide" : "tile"; }, "tile");
        const bool failed = engine.getError() != nullptr;
        engine.stop();
        return failed ? 1 : 0;
    }

  private:
    // Counts the items its cells bind by the one field every type binds.
    class CountingItems final : public ui::CollectionItems {
      public:
        [[nodiscard]] core::Json getValue(std::size_t index, std::string_view field) const override {
            binds += field == "title" ? 1 : 0;
            return ui::CollectionItems::getValue(index, field);
        }

        mutable std::uint64_t binds = 0;
    };

    static constexpr std::size_t kItems = 100000;
    static constexpr int kFlingFrames = 900;
    static constexpr int kRestFrames = 300;
    static constexpr int kFlingEvery = 45;
    static constexpr int kDragFrames = 5;
    static constexpr int kWarmupFrames = 30;
    static constexpr int kSettleFrames = 300;
    static constexpr float kFrameSeconds = 1.0F / 60.0F;

    static constexpr const char* kList = R"({"kind": "collection", "id": "items", "grow": 1, "padding": 24, "types": {
        "header": {"template": {"kind": "sectionTitle", "part": "title", "bind": {"text": "title"}}, "sticky": true},
        "plain": {"template": {"kind": "row", "gap": 16, "children": [{"kind": "label", "part": "index", "bind": {"text": "$index"}, "width": 160}, {"kind": "label", "part": "title", "bind": {"text": "title"}, "grow": 1}]}},
        "detail": {"template": {"kind": "card", "children": [{"kind": "label", "part": "title", "bind": {"text": "title"}}, {"kind": "label", "part": "caption", "font": "caption", "color": "textMuted", "text": ""}]}}
    }})";

    static constexpr const char* kGrid = R"({"kind": "collection", "id": "items", "grow": 1, "padding": 24, "layout": "grid", "minCellSize": 300, "cellAspect": 0.75, "types": {
        "header": {"template": {"kind": "sectionTitle", "part": "title", "bind": {"text": "title"}}, "span": "full", "sticky": true},
        "tile": {"template": {"kind": "card", "children": [{"kind": "label", "part": "title", "bind": {"text": "title"}}, {"kind": "label", "part": "caption", "font": "caption", "text": ""}]}},
        "wide": {"template": {"kind": "card", "children": [{"kind": "label", "part": "title", "bind": {"text": "title"}, "font": "heading"}]}, "span": 2}
    }})";

    template <typename TypeOf> static void measure(core::Engine& engine, const char* name, const char* definition, TypeOf typeOf, std::string_view boundType) {
        auto& plugin = engine.getPlugin<plugins::UiPlugin>();
        const std::shared_ptr<ui::Gui> gui = plugin.createGui(core::Json::parse(std::string(R"({"kind": "column", "children": [)") + definition + "]}"), ui::Placement::Screen);
        plugin.mount(gui);
        ui::Collection& collection = gui->getCollection("items");

        std::vector<ui::CollectionItems::Item> items(kItems);
        for (std::size_t index = 0; index < kItems; ++index) {
            items[index] = {.id = "item-" + std::to_string(index), .type = typeOf(index), .fields = {{"title", "Item " + std::to_string(index + 1)}, {"caption", "Seed " + std::to_string(index * 2654435761U % 100000)}}};
        }
        auto source = std::make_shared<CountingItems>();
        source->assign(std::move(items));
        std::uint64_t binderCalls = 0;
        // clang-format off
        collection.setBinder(boundType, [&binderCalls, &source](ui::CollectionCell& cell, std::size_t index) {
            ++binderCalls;
            cell.set("caption", {{"text", source->getValue(index, "caption")}});
        });
        // clang-format on
        collection.setSource(source);
        for (int frame = 0; frame < kWarmupFrames; ++frame) {
            engine.frame(kFrameSeconds);
        }

        const math::Rect area = collection.getBounds();
        // clang-format off
        report(engine, name, "fling", kFlingFrames, *source, binderCalls, [&engine, &area](int frame) {
            fling(engine, area, frame % kFlingEvery);
        });
        // clang-format on
        for (int frame = 0; frame < kSettleFrames; ++frame) {
            engine.frame(kFrameSeconds);
        }
        report(engine, name, "rest", kRestFrames, *source, binderCalls, [](int) {});
        plugin.unmount(*gui);
        engine.frame(kFrameSeconds);
    }

    template <typename Step> static void report(core::Engine& engine, const char* name, const char* phase, int count, const CountingItems& source, const std::uint64_t& binderCalls, Step step) {
        const debug::ObjectCounter::Snapshot cellsBefore = *debug::ObjectCounter::find("UiCell");
        const std::uint64_t bindsBefore = source.binds;
        const std::uint64_t callsBefore = binderCalls;
        double total = 0.0;
        double worst = 0.0;
        for (int frame = 0; frame < count; ++frame) {
            step(frame);
            const auto started = std::chrono::steady_clock::now();
            engine.frame(kFrameSeconds);
            const double milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            total += milliseconds;
            worst = std::max(worst, milliseconds);
        }
        const debug::ObjectCounter::Snapshot cells = *debug::ObjectCounter::find("UiCell");
        std::printf("%-6s %-6s %7d %9.3f %9.3f %8llu %10llu %8llu %7llu\n", name, phase, count, total / count, worst, static_cast<unsigned long long>(source.binds - bindsBefore), static_cast<unsigned long long>(binderCalls - callsBefore), static_cast<unsigned long long>(cells.created - cellsBefore.created), static_cast<unsigned long long>(cells.alive));
    }

    // A finger drags the content up across most of the view in the first frames of every fling and lets go, which flings it.
    static void fling(core::Engine& engine, const math::Rect& area, int step) {
        const math::Vec2 start{area.getCenter().x, area.getBottom() - 60.0F};
        const math::Vec2 moved = start - math::Vec2{0.0F, 180.0F * static_cast<float>(step)};
        if (step == 0) {
            touch(engine, platform::Event::Type::TouchBegan, start);
        } else if (step < kDragFrames) {
            touch(engine, platform::Event::Type::TouchMoved, moved);
        } else if (step == kDragFrames) {
            touch(engine, platform::Event::Type::TouchEnded, moved);
        }
    }

    static void touch(core::Engine& engine, platform::Event::Type type, math::Vec2 point) {
        platform::Event event;
        event.type = type;
        event.touchCount = 1;
        event.touches[0] = {.id = 1, .position = engine.getViewport().toFramebuffer(point + engine.getViewport().getVisibleRect().getMin()), .changed = true};
        engine.handleEvent(event);
    }
};

} // namespace haylen::bench

int main() {
    return haylen::bench::UiBenchmark::run();
}
