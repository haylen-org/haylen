#include <gtest/gtest.h>

#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_rect.h>

#include "haylen/core/AppConfig.hpp"
#include "platform/sdl/TextInput.hpp"
#include "platform/sdl/Window.hpp"

namespace haylen::platform::sdl {

// A window of the dummy video driver of SDL, whose drawable the cap of its pixel density halves, so a pixel of the framebuffer spans two units of the coordinates of the window.
class TextInputTest : public ::testing::Test {
  protected:
    void SetUp() override {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
        ASSERT_TRUE(SDL_InitSubSystem(SDL_INIT_VIDEO)) << SDL_GetError();
        window.open({.width = 400, .height = 300}, gpu::Device::Api::None);
        window.setMaxPixelDensity(0.5F);
        window.refresh();
        ASSERT_EQ(window.getPixelSize(), (math::Vec2{200.0F, 150.0F}));
    }

    void TearDown() override {
        window.close();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    [[nodiscard]] SDL_Rect getArea(int& cursor) const {
        SDL_Rect area{};
        EXPECT_TRUE(SDL_GetTextInputArea(window.get(), &area, &cursor));
        return area;
    }

    // A field whose box and caret come in framebuffer pixels, as the UI publishes them.
    [[nodiscard]] static TextInput::Field field() {
        return {.id = 7, .bounds = {50.0F, 20.0F, 100.0F, 40.0F}, .caret = {70.0F, 30.0F, 1.0F, 20.0F}};
    }

    Window window;
};

TEST_F(TextInputTest, TellsSdlTheLineOfTheCaretInTheCoordinatesOfTheWindow) {
    TextInput input(window);
    input.edit(field());
    EXPECT_TRUE(SDL_TextInputActive(window.get()));

    int cursor = -1;
    const SDL_Rect area = getArea(cursor);
    EXPECT_EQ(area.x, 100);
    EXPECT_EQ(area.y, 60);
    EXPECT_EQ(area.w, 200);
    EXPECT_EQ(area.h, 40);
    EXPECT_EQ(cursor, 40);

    input.finish();
    EXPECT_FALSE(SDL_TextInputActive(window.get()));
}

// The plain keyboard has no caret on screen, so it leaves the input method where the system puts it.
TEST_F(TextInputTest, ClearsTheAreaForThePlainKeyboard) {
    TextInput input(window);
    input.edit(field());
    input.edit({.id = TextInput::kKeyboardField});
    EXPECT_TRUE(SDL_TextInputActive(window.get()));

    int cursor = -1;
    const SDL_Rect area = getArea(cursor);
    EXPECT_EQ(area.w, 0);
    EXPECT_EQ(area.h, 0);
    EXPECT_EQ(cursor, 0);
}

// An input method that forgot the place of the caret while the window lost the focus gets it again, and one that no field edits for gets nothing.
TEST_F(TextInputTest, AppliesTheAreaAgainOnceTheWindowGainsTheFocus) {
    TextInput input(window);
    input.edit(field());
    const SDL_Rect forgotten{.x = 1, .y = 2, .w = 3, .h = 4};
    ASSERT_TRUE(SDL_SetTextInputArea(window.get(), &forgotten, 5));

    input.restore();
    int cursor = -1;
    SDL_Rect area = getArea(cursor);
    EXPECT_EQ(area.x, 100);
    EXPECT_EQ(area.w, 200);
    EXPECT_EQ(cursor, 40);

    input.finish();
    ASSERT_TRUE(SDL_SetTextInputArea(window.get(), &forgotten, 5));
    input.restore();
    area = getArea(cursor);
    EXPECT_EQ(area.x, 1);
    EXPECT_EQ(cursor, 5);
}

} // namespace haylen::platform::sdl
