#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

std::map<std::string, std::string> fontFiles() {
    const std::vector<std::uint8_t> image = test::pngImage(32, 16, 0xFFFFFFFFU);
    const std::string png(image.begin(), image.end());
    const std::string bmfont = "info face=\"Pixel\" size=8\ncommon lineHeight=10 base=8 pages=1\npage id=0 file=\"pixel.png\"\nchar id=65 x=0 y=0 width=8 height=8 xoffset=0 yoffset=0 xadvance=9 page=0\nchar id=66 x=8 y=0 width=8 height=8 xoffset=0 yoffset=0 xadvance=9 page=0\nkerning first=65 second=66 amount=-1\n";
    return {{"content/fonts/pixel.fnt", bmfont}, {"content/fonts/pixel.png", png}, {"content/images/coin.png", png}};
}

} // namespace

class TextLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture.runLua("graphics = require('haylen.graphics') graphics2d = require('haylen.graphics2d') assets = require('haylen.assets') ui = require('haylen.ui') font = graphics2d.defaultFont()");
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    // Runs the Lua body inside a scene render callback for one frame and reports the first error.
    std::string render(const std::string& body) {
        lua("require('haylen.scene').clear() renderError = nil require('haylen.scene').push({render = function() local ok, message = pcall(function() " + body + " end) if not ok then renderError = message end end})");
        fixture.frames(1);
        return lua("return tostring(renderError)");
    }

    test::EngineFixture fixture{fontFiles()};
};

TEST_F(TextLuaTest, InspectsFontsAndBuildsFamilies) {
    EXPECT_EQ(lua("return tostring(font.distanceField) .. ' ' .. font.nativeSize .. ' ' .. font.pageCount .. ' ' .. tostring(font:hasGlyph('A')) .. ' ' .. tostring(font:hasGlyph(0xE000))"), "true 48.0 1 true false");
    EXPECT_EQ(lua("local glyph = font:glyph('A') return tostring(glyph.visible) .. ' ' .. tostring(glyph.advance > 0) .. ' ' .. glyph.page .. ' ' .. tostring(font:page(1).width >= 512)"), "true true 1 true");
    EXPECT_EQ(lua("return font:toDistance(4, 32) == 2 * font:toDistance(2, 32) and font:kerning('A', 'V') <= 0"), "true");
    EXPECT_NE(lua("font:glyph('AB')").find("expected one character or a code point"), std::string::npos);
    EXPECT_NE(lua("font:page(2)").find("page out of range"), std::string::npos);

    lua("pixel = assets.font('fonts/pixel.fnt') grid = graphics.newGridFont(assets.texture('images/coin.png'), {characters = 'ABCDEFGH', cellWidth = 8, cellHeight = 8, spacing = {0, 0}})");
    EXPECT_EQ(lua("return tostring(pixel.distanceField) .. ' ' .. pixel.nativeSize .. ' ' .. pixel:kerning('A', 'B') .. ' ' .. pixel:lineHeight(16) .. ' ' .. tostring(grid:hasGlyph('H'))"), "false 8.0 -1.0 20.0 true");
    EXPECT_EQ(lua("local layout = pixel:layout('AB', {size = 8}) return #layout.quads .. ' ' .. layout.quads[2].position.x .. ' ' .. layout.quads[2].page"), "2 8.0 1");
    EXPECT_EQ(lua("local loaded = assets.load('images/coin.png', 'gridFont', {characters = 'XY', cellWidth = 16, cellHeight = 16}) return tostring(loaded:hasGlyph('Y'))"), "true");
    EXPECT_EQ(lua("local data = assets.text('fonts/pixel.fnt') local made = graphics.newBitmapFont(data, {assets.texture('fonts/pixel.png')}) return made:measure('AB', {size = 8})"), "17.0");
    EXPECT_NE(lua("assets.font('images/coin.png')").find("expected a .ttf, .otf or .fnt file"), std::string::npos);
    EXPECT_NE(lua("graphics.newBitmapFont('garbage', {})").find("info and common"), std::string::npos);

    lua("family = graphics.newFontFamily({regular = font, bold = pixel, fallback = {grid}})");
    EXPECT_EQ(lua("return tostring(family.regular == nil) .. ' ' .. tostring(family.italic == nil) .. ' ' .. #family.fallback .. ' ' .. tostring(family.bold.distanceField)"), "false true 1 false");
    EXPECT_EQ(lua("local face, bold, italic = family:select({bold = true, italic = true}) return tostring(face.distanceField) .. ' ' .. tostring(bold) .. ' ' .. tostring(italic)"), "false false true");
    EXPECT_EQ(lua("local face, bold = family:select({bold = true}) local regular, synthetic = family:select() return tostring(bold) .. ' ' .. tostring(synthetic) .. ' ' .. tostring(regular.distanceField)"), "false false true");
    EXPECT_EQ(lua("local face, bold = family:resolve(0xE000) return tostring(face == nil) .. ' ' .. tostring(bold)"), "false false");
    EXPECT_EQ(lua("local face, bold, italic = family:resolve('H', {italic = true}) return tostring(face.distanceField) .. ' ' .. tostring(italic)"), "true true");
    EXPECT_NE(lua("graphics.newFontFamily({bold = font})").find("A font family needs a regular face."), std::string::npos);
    EXPECT_NE(lua("graphics.newFontFamily({regular = font, heavy = font})").find("Unknown option 'heavy'"), std::string::npos);
}

TEST_F(TextLuaTest, SameFontsCompareEqual) {
    lua("pixel = assets.font('fonts/pixel.fnt') family = graphics.newFontFamily({regular = font, fallback = {pixel}}) target = graphics.newRenderTarget(8, 8)");
    EXPECT_EQ(lua("return tostring(family.fallback[1] == family.fallback[1]) .. ' ' .. tostring(family:resolve('A') == family.regular) .. ' ' .. tostring(family.regular == font) .. ' ' .. tostring(pixel == assets.font('fonts/pixel.fnt'))"), "true true true true");
    EXPECT_EQ(lua("return tostring(pixel == font) .. ' ' .. tostring(family == graphics.newFontFamily({regular = font})) .. ' ' .. tostring(target == graphics.newRenderTarget(8, 8))"), "false false false");

    // Values of different types are never equal, and comparing them raises no error.
    EXPECT_EQ(lua("return tostring(font == family) .. ' ' .. tostring(target == target.texture) .. ' ' .. tostring(graphics.whiteTexture() == font)"), "false false false");
}

TEST_F(TextLuaTest, MakesDrawsAndMeasuresRichText) {
    lua("text = graphics2d.newRichText('[b]Hi[/b] [url=next]there[/url] [hint=Tip]you[/hint]', {size = 24, maxWidth = 300, align = 'center', color = '#FFFFFF'})");
    EXPECT_EQ(lua("local w, h = text:size() return w .. ' ' .. tostring(h > 20) .. ' ' .. text.characterCount .. ' ' .. text.markup"), "300.0 true 12 [b]Hi[/b] [url=next]there[/url] [hint=Tip]you[/hint]");
    EXPECT_EQ(lua("local layout = text:layout() local link = layout.links[1].rect return text:linkAt(link.x + 2, link.y + 2) .. ' ' .. tostring(text:linkAt(0, 0)) .. ' ' .. text:hintAt(layout.hints[1].rect.x + 1, layout.hints[1].rect.y + 1)"), "next nil Tip");
    EXPECT_EQ(lua("local layout = text:layout() return #layout.glyphs .. ' ' .. layout.lineCount .. ' ' .. layout.glyphs[1].char .. ' ' .. tostring(layout.glyphs[1].syntheticBold) .. ' ' .. layout.boxes[1].kind .. ' ' .. layout.links[1].link"), "10 1 H true underline next");

    lua("text.maxWidth = 40 text.scale = 2");
    EXPECT_EQ(lua("local w, h = text:size() return w .. ' ' .. tostring(h > 100) .. ' ' .. text.maxWidth .. ' ' .. text.scale"), "40.0 true 40.0 2.0");
    lua("text.markup = 'abc'");
    EXPECT_EQ(lua("return text.characterCount .. ' ' .. text.visibleCharacters .. ' ' .. tostring(text.revealing)"), "3 3 false");
    lua("text:setVisibleCharacters(1)");
    EXPECT_EQ(lua("return text.visibleCharacters .. ' ' .. tostring(text.revealing) .. ' ' .. tostring(text:layout().glyphs[2].visible)"), "1 true false");
    lua("text.visibleRatio = 1 text.visibleCharacters = -1");
    EXPECT_EQ(lua("return text.visibleCharacters .. ' ' .. text.visibleRatio"), "3 1.0");

    lua("typed = graphics2d.newRichText('abcd', {reveal = 20})");
    EXPECT_EQ(lua("typed:update(0.11) return typed.visibleCharacters .. ' ' .. string.format('%.2f', typed.time)"), "2 0.11");

    EXPECT_EQ(render("graphics2d.beginScreen() text:draw(10, 20, {layer = 1}) graphics2d.drawRichText('[wave]hello[/wave] [img=images/coin.png]', 10, 60, {size = 20, layer = 2})"), "nil");
    EXPECT_EQ(lua("return graphics2d.stats().sprites"), "9");
    EXPECT_EQ(lua("local w, h = graphics2d.measureRichText('[size=40]big[/size]', {family = font}) local small = graphics2d.measureRichText('big') return tostring(w > small) .. ' ' .. tostring(h >= 40)"), "true true");

    EXPECT_NE(lua("graphics2d.newRichText('[b]open')").find("[b] is never closed."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {sizes = 3})").find("Unknown option 'sizes'"), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {family = 3})").find("expected a FontFamily or a Font"), std::string::npos);
    EXPECT_NE(render("graphics2d.drawRichText('x', 0, 0)").find("No canvas is active"), std::string::npos);
}

TEST_F(TextLuaTest, RegistersEffectsIconsAndFonts) {
    lua("graphics2d.registerTextEffect('lift', function(glyph, attributes) glyph.offsetY = glyph.offsetY - attributes.by * glyph.index glyph.visible = glyph.char ~= 'x' glyph.color = '#FF0000' end)");
    lua("lifted = graphics2d.newRichText('[lift by=3]axb[/lift]c')");
    EXPECT_EQ(lua("local moved, still = lifted:layout(), graphics2d.newRichText('axbc'):layout() return string.format('%.1f %.1f', still.glyphs[1].rect.y - moved.glyphs[1].rect.y, still.glyphs[3].rect.y - moved.glyphs[3].rect.y) .. ' ' .. tostring(moved.glyphs[2].visible) .. ' ' .. moved.glyphs[1].color:toHex() .. ' ' .. tostring(still.glyphs[4].rect.y == moved.glyphs[4].rect.y)"), "3.0 9.0 false #FFFF0000 true");
    EXPECT_NE(lua("local names = table.concat(graphics2d.textEffects(), ',') return names").find("lift"), std::string::npos);
    EXPECT_NE(lua("graphics2d.registerTextEffect('wave', function() end)").find("[wave] is a built-in text effect."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('[unknown]x[/unknown]')").find("[unknown] is neither a tag nor a registered text effect."), std::string::npos);

    lua("graphics2d.registerTextEffect('broken', function(glyph) error('effect failed') end)");
    EXPECT_NE(lua("graphics2d.newRichText('[broken]x[/broken]'):layout()").find("effect failed"), std::string::npos);

    lua("graphics2d.registerTextIcon('coin', assets.texture('images/coin.png'), {source = {0, 0, 16, 16}, width = 20, height = 20})");
    EXPECT_EQ(lua("local layout = graphics2d.newRichText('pay [icon=coin] [icon=coin height=10]'):layout() return layout.images[1].rect.width .. ' ' .. layout.images[2].rect.width"), "20.0 10.0");
    EXPECT_NE(lua("graphics2d.registerTextIcon('bad', nil)").find("error"), std::string::npos);

    lua("family = graphics.newFontFamily({regular = font}) ui.addFont('story', family) ui.addFont('pixel', assets.font('fonts/pixel.fnt'))");
    EXPECT_EQ(lua("local story = graphics2d.newRichText('[font=serif]x[/font]', {fonts = {serif = family}}) return story.characterCount"), "1");
    EXPECT_NE(lua("ui.addFont('story', family)").find("already has a font named story"), std::string::npos);
}

} // namespace haylen
