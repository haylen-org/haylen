#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/plugins/UiPlugin.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/Screen.hpp"

namespace haylen::test {

// Screen that builds its GUI from a JSON tree and writes every hook it receives to a shared log as `name:hook`. Its back hook keeps it while `keeping` is set.
class RecordingScreen final : public ui::Screen {
  public:
    RecordingScreen(plugins::UiPlugin& owner, std::string label, std::vector<std::string>& entries, std::string json = R"({"kind": "button", "id": "open", "text": "Open", "align": "stretch"})") : ui(owner), name(std::move(label)), log(entries), tree(std::move(json)) {}

    [[nodiscard]] std::shared_ptr<ui::Gui> build(core::Engine&, ui::Placement placement) override {
        record("build");
        ++builds;
        return ui.createGui(core::Json::parse(tree), placement);
    }
    void appear(core::Engine&) override {
        record("appear");
    }
    void appeared(core::Engine&) override {
        record("appeared");
    }
    void disappear(core::Engine&) override {
        record("disappear");
    }
    void disappeared(core::Engine&) override {
        record("disappeared");
    }
    void update(core::Engine&, float) override {
        ++updates;
    }
    [[nodiscard]] bool back(core::Engine&) override {
        record("back");
        return keeping;
    }
    void remove(core::Engine&) override {
        record("remove");
    }

    [[nodiscard]] const std::string& getName() const noexcept {
        return name;
    }

    int builds = 0;
    int updates = 0;
    bool keeping = false;

  private:
    void record(const char* hook) {
        log.push_back(name + ":" + hook);
    }

    plugins::UiPlugin& ui;
    std::string name;
    std::vector<std::string>& log;
    std::string tree;
};

} // namespace haylen::test
