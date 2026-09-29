#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/EmbeddedFiles.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Viewport.hpp"
#include "haylen/input/Input.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/ui/Backend.hpp"
#include "haylen/ui/NavigationInput.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::ui {

namespace {

class BackendTest : public ::testing::Test {
  protected:
    BackendTest() : backend(getEngine().getGraphics(), getEngine().getWindow(), core::EmbeddedFiles::getDefaultFont()) {}

    core::Engine& getEngine() {
        return fixture.engine();
    }

    void frame(const std::function<void()>& build) {
        getEngine().getRenderer2D().beginFrame(getEngine().getViewport(), math::Color::black());
        backend.beginFrame(1.0F / 60.0F, getEngine().getViewport(), getEngine().getInput(), navigation);
        build();
        backend.render(getEngine().getRenderer2D());
        getEngine().getRenderer2D().endFrame(fixture.host().getFrameTarget());
    }

    void send(platform::Event::Type type, math::Vec2 position = {}) {
        platform::Event event;
        event.type = type;
        event.position = position;
        backend.handleEvent(event, getEngine().getViewport());
    }

    static void beginWindow(const char* name, math::Vec2 position, ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration) {
        ImGui::SetNextWindowPos({position.x, position.y});
        ImGui::SetNextWindowSize({400.0F, 200.0F});
        ImGui::Begin(name, nullptr, flags);
    }

    test::EngineFixture fixture;
    Backend backend;
    NavigationInput navigation;
};

} // namespace

TEST_F(BackendTest, DrawsMeshesAndReportsClicks) {
    int clicks = 0;
    // clang-format off
    const auto build = [&] {
        beginWindow("menu", {100.0F, 100.0F});
        clicks += ImGui::Button("Play", {200.0F, 60.0F}) ? 1 : 0;
        ImGui::End();
    };
    // clang-format on

    frame(build);
    EXPECT_GT(getEngine().getRenderer2D().getStats().vertices, 0U);
    EXPECT_GT(getEngine().getRenderer2D().getStats().indices, 0U);
    EXPECT_EQ(backend.getDisplayRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));
    EXPECT_EQ(backend.getSafeRect(), (math::Rect{0.0F, 0.0F, 1920.0F, 1080.0F}));

    send(platform::Event::Type::MouseMove, {150.0F, 130.0F});
    frame(build);
    EXPECT_TRUE(backend.isUsingPointer());
    send(platform::Event::Type::MouseDown, {150.0F, 130.0F});
    frame(build);
    send(platform::Event::Type::MouseUp, {150.0F, 130.0F});
    frame(build);
    frame(build);
    EXPECT_EQ(clicks, 1);

    send(platform::Event::Type::MouseMove, {1500.0F, 900.0F});
    frame(build);
    frame(build);
    EXPECT_FALSE(backend.isUsingPointer());
}

TEST_F(BackendTest, TypesTextThroughKeyboardEvents) {
    std::string name;
    bool focus = true;
    // clang-format off
    const auto build = [&] {
        beginWindow("form", {0.0F, 0.0F});
        if (std::exchange(focus, false)) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputText("##name", &name);
        ImGui::End();
    };
    // clang-format on
    frame(build);
    frame(build);
    for (const char32_t character : std::u32string(U"Ana é")) {
        platform::Event event;
        event.type = platform::Event::Type::Character;
        event.character = character;
        backend.handleEvent(event, getEngine().getViewport());
        frame(build);
    }
    frame(build);
    EXPECT_EQ(name, "Ana \xC3\xA9");
    EXPECT_TRUE(backend.isUsingKeyboard());
    EXPECT_TRUE(fixture.host().isKeyboardVisible());
}

TEST_F(BackendTest, PassesThePointerThroughTransparentWindows) {
    // clang-format off
    const auto build = [&] {
        beginWindow("hud", {0.0F, 0.0F}, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        backend.setTransparentWindow();
        ImGui::SetCursorScreenPos({300.0F, 100.0F});
        (void)ImGui::Button("Pause", {80.0F, 40.0F});
        backend.blockPointer({20.0F, 20.0F, 60.0F, 60.0F});
        ImGui::End();
    };
    // clang-format on

    send(platform::Event::Type::MouseMove, {200.0F, 150.0F});
    frame(build);
    frame(build);
    EXPECT_FALSE(backend.isUsingPointer());

    send(platform::Event::Type::MouseMove, {40.0F, 40.0F});
    frame(build);
    frame(build);
    EXPECT_TRUE(backend.isUsingPointer());

    send(platform::Event::Type::MouseMove, {320.0F, 120.0F});
    frame(build);
    frame(build);
    EXPECT_TRUE(backend.isUsingPointer());
}

TEST_F(BackendTest, RecoversFromFramesLeftOpenAndReportsMisuse) {
    EXPECT_THROW(frame([] { ImGui::End(); }), std::logic_error);

    getEngine().getRenderer2D().beginFrame(getEngine().getViewport(), math::Color::black());
    backend.beginFrame(1.0F / 60.0F, getEngine().getViewport(), getEngine().getInput(), navigation);
    beginWindow("broken", {0.0F, 0.0F});
    EXPECT_TRUE(backend.isFrameActive());
    getEngine().getRenderer2D().endFrame(fixture.host().getFrameTarget());

    bool drawn = false;
    // clang-format off
    frame([&] {
        beginWindow("fine", {0.0F, 0.0F});
        drawn = true;
        ImGui::End();
    });
    // clang-format on
    EXPECT_TRUE(drawn);
    EXPECT_FALSE(backend.isFrameActive());
}

TEST_F(BackendTest, ManagesFontsAndAppTextures) {
    EXPECT_EQ(backend.getFont("default"), backend.getDefaultFont());
    EXPECT_THROW((void)backend.getFont("missing"), std::invalid_argument);
    EXPECT_THROW(backend.addFont("default", {}), std::invalid_argument);
    EXPECT_THROW(backend.addFont("broken", {1, 2, 3}), std::runtime_error);

    const graphics::Texture& white = getEngine().getGraphics().getWhiteTexture();
    // clang-format off
    frame([&] {
        beginWindow("images", {0.0F, 0.0F});
        ImGui::Image(backend.getTextureReference(white), {32.0F, 32.0F});
        ImGui::End();
    });
    // clang-format on
    EXPECT_GE(getEngine().getRenderer2D().getStats().drawCalls, 1U);
}

} // namespace haylen::ui
