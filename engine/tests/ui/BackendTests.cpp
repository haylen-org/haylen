#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <numbers>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

    [[nodiscard]] static std::vector<std::uint8_t> readFont(const std::string& name) {
        if (name == "default") {
            const std::span<const std::uint8_t> data = core::EmbeddedFiles::getDefaultFont();
            return {data.begin(), data.end()};
        }
        std::ifstream file(std::string(HAYLEN_TEST_FONTS) + "/" + name, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
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

// The curves ImGui draws itself, such as the corners and widgets of the windows of "haylen.imgui", stay within a fifth of a pixel of the true curve and fade over one pixel of the screen at every scale of the UI.
TEST_F(BackendTest, FadesCurvesOverOnePixelAtEveryScale) {
    constexpr math::Vec2 kCenter{200.0F, 150.0F};
    constexpr float kRadius = 16.0F;
    for (const float scale : {0.5F, 1.0F, 2.0F, 3.0F}) {
        backend.setScale(scale);
        const ImDrawList* list = nullptr;
        int first = 0;
        float pixels = 0.0F;
        // clang-format off
        frame([&] {
            beginWindow("curves", {0.0F, 0.0F});
            list = ImGui::GetWindowDrawList();
            first = list->VtxBuffer.Size;
            ImGui::GetWindowDrawList()->AddCircleFilled({kCenter.x, kCenter.y}, kRadius, IM_COL32_WHITE);
            pixels = ImGui::GetIO().DisplayFramebufferScale.x;
            ImGui::End();
        });
        // clang-format on

        // Every point of the outline has a vertex half a pixel inside it with the full color and one half a pixel outside it that is clear.
        ASSERT_NE(list, nullptr);
        const int count = (list->VtxBuffer.Size - first) / 2;
        ASSERT_GT(count, 8) << scale;
        for (int point = 0; point < count; ++point) {
            const ImDrawVert& inner = list->VtxBuffer[first + point * 2];
            const ImDrawVert& outer = list->VtxBuffer[first + point * 2 + 1];
            const float innerDistance = std::hypot(inner.pos.x - kCenter.x, inner.pos.y - kCenter.y);
            const float outerDistance = std::hypot(outer.pos.x - kCenter.x, outer.pos.y - kCenter.y);
            EXPECT_NEAR((outerDistance - innerDistance) * pixels, 1.0F, 0.05F) << scale;
            EXPECT_EQ(outer.col >> IM_COL32_A_SHIFT, 0U);
            EXPECT_EQ(inner.col >> IM_COL32_A_SHIFT, 255U);
        }
        const float stray = kRadius * (1.0F - std::cos(std::numbers::pi_v<float> / static_cast<float>(count)));
        EXPECT_LE(stray * pixels, 0.2F) << scale;
    }
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
    EXPECT_TRUE(backend.hasFont(Backend::kDefaultFontName));
    EXPECT_THROW((void)backend.getFont("missing"), std::invalid_argument);
    EXPECT_THROW(backend.addFont("default", {}), std::invalid_argument);
    EXPECT_THROW(backend.addFont("broken", {.regular = {1, 2, 3}}), std::runtime_error);
    EXPECT_THROW(backend.addFont("broken", {.regular = readFont("default"), .fallbacks = {{1, 2, 3}}}), std::runtime_error);
    EXPECT_FALSE(backend.hasFont("broken"));

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

// A child window draws at its place among the items of its parent, so what the parent draws after it, such as the GUIs above a scroll, covers it.
TEST_F(BackendTest, DrawsChildWindowsBetweenTheItemsAroundThem) {
    std::vector<std::string> order;
    const auto mark = [&](const char* name) { backend.addRenderCallback([&order, name](graphics2d::Renderer&) { order.emplace_back(name); }); };
    // clang-format off
    const auto build = [&] {
        beginWindow("page", {0.0F, 0.0F});
        mark("before");
        if (backend.beginChild("##list", {200.0F, 100.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
            mark("list");
            if (backend.beginChild("##row", {100.0F, 40.0F}, ImGuiChildFlags_None, ImGuiWindowFlags_None)) {
                mark("row");
            }
            ImGui::EndChild();
            mark("list end");
        }
        ImGui::EndChild();
        mark("after");
        ImGui::End();
    };
    // clang-format on

    frame(build);
    EXPECT_EQ(order, (std::vector<std::string>{"before", "list", "row", "list end", "after"}));
}

// Every face of a font draws the characters it lacks from the fallbacks, whose em squares match the em square of the face, and a style the font has no face for draws with the regular face.
TEST_F(BackendTest, DrawsMissingCharactersFromFallbacksAtTheEmSize) {
    ImFont* regular = backend.addFont("story", {.regular = readFont("default"), .bold = readFont("crimson_text_bold.ttf"), .fallbacks = {readFont("mplus_1p_regular.ttf"), readFont("noto_sans_symbols_2_regular.ttf")}});
    EXPECT_EQ(backend.getFont("story"), regular);
    EXPECT_NE(backend.getFont("story", true), regular);
    EXPECT_EQ(backend.getFont("story", true, true), backend.getFont("story", true));
    EXPECT_EQ(backend.getFont("story", false, true), regular);

    // The default font is 2048 units to the em and 2400 from ascent to descent, Crimson Text 1024 to the em and 1331 from ascent to descent, and a Japanese character advances by one em.
    EXPECT_NEAR(backend.getEmSize("story", 30.0F), 30.0F * 2048.0F / 2400.0F, 0.001F);
    // clang-format off
    frame([&] {
        for (const auto& [face, em] : {std::pair{regular, 30.0F * 2048.0F / 2400.0F}, std::pair{backend.getFont("story", true), 30.0F * 1024.0F / 1331.0F}}) {
            EXPECT_TRUE(face->IsGlyphInFont(U'日'));
            EXPECT_TRUE(face->IsGlyphInFont(U'☀'));
            EXPECT_NEAR(face->GetFontBaked(30.0F)->GetCharAdvance(U'日'), em, 0.5F);
        }
        EXPECT_FALSE(backend.getFont(Backend::kDefaultFontName)->IsGlyphInFont(U'日'));
    });
    // clang-format on
}

} // namespace haylen::ui
