#include <gtest/gtest.h>

#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

// A bitmap font, an image and the Hebrew test font.
std::map<std::string, std::string> fontFiles() {
    const std::vector<std::uint8_t> image = test::pngImage(32, 16, 0xFFFFFFFFU);
    const std::string png(image.begin(), image.end());
    const std::string bmfont = "info face=\"Pixel\" size=8\ncommon lineHeight=10 base=8 pages=1\npage id=0 file=\"pixel.png\"\nchar id=65 x=0 y=0 width=8 height=8 xoffset=0 yoffset=0 xadvance=9 page=0\nchar id=66 x=8 y=0 width=8 height=8 xoffset=0 yoffset=0 xadvance=9 page=0\nkerning first=65 second=66 amount=-1\n";
    std::ifstream hebrew(std::string(HAYLEN_TEST_FONTS) + "/noto_sans_hebrew_regular.ttf", std::ios::binary);
    return {{"content/fonts/pixel.fnt", bmfont}, {"content/fonts/pixel.png", png}, {"content/images/coin.png", png}, {"content/fonts/hebrew.ttf", {std::istreambuf_iterator<char>(hebrew), std::istreambuf_iterator<char>()}}};
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
    EXPECT_EQ(lua("local shaped = font:shape('AV', {size = 96}) return font:toDistance(4, 32) == 2 * font:toDistance(2, 32) and #shaped == 2 and shaped[2].cluster == 2 and shaped[1].advance <= font:glyph('A').advance * 2"), "true");
    EXPECT_NE(lua("font:glyph('AB')").find("expected one character or a code point"), std::string::npos);
    EXPECT_NE(lua("font:page(2)").find("page out of range"), std::string::npos);

    lua("pixel = assets.font('fonts/pixel.fnt') grid = graphics.newGridFont(assets.texture('images/coin.png'), {characters = 'ABCDEFGH', cellWidth = 8, cellHeight = 8, spacing = {0, 0}})");
    EXPECT_EQ(lua("return tostring(pixel.distanceField) .. ' ' .. pixel.nativeSize .. ' ' .. pixel:shape('AB')[1].advance - pixel:glyph('A').advance .. ' ' .. pixel:lineHeight(16) .. ' ' .. tostring(grid:hasGlyph('H'))"), "false 8.0 -1.0 20.0 true");
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

    // A glyph index from shaping reads its glyph back, and a bitmap font numbers its glyphs by code point.
    EXPECT_EQ(lua("local shaped = font:shape('A') local glyph = font:glyphByIndex(shaped[1].index) return tostring(glyph.index == shaped[1].index and glyph.advance == font:glyph('A').advance) .. ' ' .. pixel:glyphByIndex(65).advance"), "true 9.0");
    EXPECT_NE(lua("font:glyphByIndex(1000000)").find("The font has no glyph with index 1000000."), std::string::npos);
    EXPECT_NE(lua("font:glyphByIndex(-1)").find("expected a glyph index"), std::string::npos);
}

TEST_F(TextLuaTest, RejectsFontOptionsAndLengthsOutOfRange) {
    EXPECT_NE(lua("assets.font('fonts/hebrew.ttf', {bakeSize = 0})").find("The bake size of a TrueType font must be positive and at most the maximum texture size of"), std::string::npos);
    EXPECT_NE(lua("assets.font('fonts/hebrew.ttf', {spread = 0})").find("The spread of a TrueType font must be positive"), std::string::npos);
    EXPECT_NE(lua("assets.font('fonts/hebrew.ttf', {atlasSize = 100000})").find("The atlas size of a TrueType font must be positive"), std::string::npos);
    EXPECT_NE(lua("graphics.newGridFont(assets.texture('images/coin.png'), {characters = 'A', cellWidth = 8, cellHeight = 8, margin = {20, 20}})").find("The grid font image holds 0 cells, fewer than its 1 characters."), std::string::npos);

    // A length that is not a number could never be found in the layout cache again.
    EXPECT_NE(lua("graphics2d.measureText(nil, 'x', {size = 0 / 0})").find("Text needs a finite size, maximum width and line spacing."), std::string::npos);
    EXPECT_NE(lua("font:layout('x', {maxWidth = math.huge})").find("Text needs a finite size, maximum width and line spacing."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {maxWidth = 0 / 0})").find("Rich text needs a finite maximum width."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x').maxWidth = math.huge").find("Rich text needs a finite maximum width."), std::string::npos);
}

TEST_F(TextLuaTest, EndsParagraphsAtEverySeparatorAndHidesControlCharacters) {
    EXPECT_EQ(lua(R"(local function lines(text) return font:layout(text, {size = 20}).lineCount end return lines('a\rb') .. ' ' .. lines('a\r\nb') .. ' ' .. lines('a\u{2029}b\u{85}c\u{1C}d') .. ' ' .. lines('a\u{2028}b') .. ' ' .. lines('a\vb'))"), "2 2 4 2 2");
    EXPECT_EQ(lua(R"(return graphics2d.newRichText('a\r\nb\rc\u{2029}d'):layout().lineCount .. ' ' .. graphics2d.newRichText('[center]\r\nx\r\n[/center]\r\ny'):layout().lineCount)"), "4 2");

    // A tab draws no missing glyph box and takes no room.
    EXPECT_EQ(lua(R"(local laid = font:layout('a\tb', {size = 20}) return #laid.quads .. ' ' .. tostring(math.abs(laid.size.x - font:measure('a', {size = 20}) - font:measure('b', {size = 20})) < 0.01))"), "2 true");
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

    // The bold and italic options style all the text as [b] and [i] would, here synthesized from the default font.
    EXPECT_EQ(lua("local glyph = graphics2d.newRichText('ab', {bold = true, italic = true}):layout().glyphs[2] return tostring(glyph.syntheticBold) .. ' ' .. tostring(glyph.syntheticItalic)"), "true true");

    lua("typed = graphics2d.newRichText('abcd', {reveal = 20})");
    EXPECT_EQ(lua("typed:update(0.11) return typed.visibleCharacters .. ' ' .. string.format('%.2f', typed.time)"), "2 0.11");

    EXPECT_EQ(render("graphics2d.beginScreen() text:draw(10, 20, {layer = 1}) graphics2d.drawRichText('[wave]hello[/wave] [img=images/coin.png]', 10, 60, {size = 20, layer = 2})"), "nil");
    EXPECT_EQ(lua("return graphics2d.stats().sprites"), "9");
    EXPECT_EQ(lua("local w, h = graphics2d.measureRichText('[size=40]big[/size]', {family = font}) local small = graphics2d.measureRichText('big') return tostring(w > small) .. ' ' .. tostring(h >= 40)"), "true true");

    EXPECT_NE(lua("graphics2d.newRichText('[b]open')").find("[b] is never closed."), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {sizes = 3})").find("Unknown option 'sizes'"), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {family = 3})").find("expected a FontFamily or a Font"), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('x', {fonts = {graphics.newFontFamily({regular = font})}})").find("The fonts option maps font names to families, so its keys must be strings."), std::string::npos);
    EXPECT_NE(render("graphics2d.drawRichText('x', 0, 0)").find("No canvas is active"), std::string::npos);
}

TEST_F(TextLuaTest, ChangesRichTextOptionsAsProperties) {
    lua("story = graphics2d.newRichText('ab', {reveal = 10}) story:update(0.15)");
    lua("story.color = '#FFFF0000' story.bold = true story.italic = true story.align = 'center' story.direction = 'rtl' story.language = 'he' story.lineSpacing = 2 story.underlineLinks = false");
    EXPECT_EQ(lua("return story.color:toHex() .. ' ' .. tostring(story.bold) .. ' ' .. tostring(story.italic) .. ' ' .. story.align .. ' ' .. story.direction .. ' ' .. story.language .. ' ' .. story.lineSpacing .. ' ' .. tostring(story.underlineLinks) .. ' ' .. story.reveal"), "#FFFF0000 true true center rtl he 2.0 false 10.0");

    // Changing an option lays the text out again and starts its reveal over.
    EXPECT_EQ(lua("local glyph = story:layout().glyphs[1] return story.visibleCharacters .. ' ' .. tostring(glyph.syntheticBold) .. ' ' .. glyph.color:toHex()"), "0 true #FFFF0000");
    EXPECT_EQ(lua("story.reveal = 0 story.family = assets.font('fonts/pixel.fnt') return story.visibleCharacters .. ' ' .. tostring(story.family.regular == assets.font('fonts/pixel.fnt'))"), "2 true");
    EXPECT_NE(lua("story.lineSpacing = 0").find("Rich text needs a positive size, scale and line spacing."), std::string::npos);

    // The size at another width measures the text without changing its own width.
    EXPECT_EQ(lua("local wide = graphics2d.newRichText('one two three') local width = wide:size(40) return width .. ' ' .. tostring(wide:size() > 40) .. ' ' .. wide.maxWidth"), "40.0 true 0.0");
}

TEST_F(TextLuaTest, ShapesAndOrdersRightToLeftText) {
    lua("hebrew = assets.font('fonts/hebrew.ttf') scripts = graphics.newFontFamily({regular = font, fallback = {hebrew}})");

    // Left-to-right text puts the Hebrew word after it reversed, drawn by the fallback, and the first Hebrew letter stands last on the right.
    EXPECT_EQ(lua("local laid = scripts:layout('abc שלום', {size = 20}) local last = laid.quads[#laid.quads] return #laid.quads .. ' ' .. tostring(laid.quads[1].font == font) .. ' ' .. tostring(last.font == hebrew) .. ' ' .. laid.lineCount"), "7 true true 1");
    EXPECT_EQ(lua("local w, h = scripts:measure('שלום', {size = 20}) local laid = scripts:layout('שלום', {size = 20}) return tostring(w == laid.size.x and h == laid.size.y)"), "true");
    EXPECT_EQ(lua("local w = graphics2d.measureText(scripts, 'שלום abc', {size = 20, maxWidth = 50, direction = 'rtl', language = 'he'}) return w"), "50.0");
    EXPECT_EQ(lua("local shaped = hebrew:shape('שלום', {direction = 'rtl', size = 40}) return #shaped .. ' ' .. shaped[1].cluster .. ' ' .. shaped[4].cluster .. ' ' .. tostring(shaped[1].advance > 0)"), "4 4 1 true");
    EXPECT_EQ(lua("return hebrew:glyph('ש').index .. ' ' .. tostring(hebrew:glyph('ש').visible)"), lua("return hebrew:shape('ש')[1].index .. ' true'"));

    // Rich text reads a paragraph right to left when asked or from its first letter, and markup sets the direction of a paragraph.
    EXPECT_EQ(lua("local laid = graphics2d.newRichText('שלום abc', {family = scripts, direction = 'rtl', language = 'he'}):layout() return tostring(laid.lines[1].rightToLeft) .. ' ' .. tostring(laid.characters[1].rightToLeft) .. ' ' .. tostring(laid.characters[6].rightToLeft) .. ' ' .. laid.characters[6].first .. '-' .. laid.characters[6].last"), "true true false 6-6");
    EXPECT_EQ(lua("local laid = graphics2d.newRichText('abc\\n[p dir=rtl align=start]abc[/p]', {family = scripts, maxWidth = 200}):layout() return tostring(laid.lines[1].rightToLeft) .. ' ' .. tostring(laid.lines[2].rightToLeft) .. ' ' .. tostring(laid.lines[2].rect.x > 100)"), "false true true");
    EXPECT_EQ(render("graphics2d.beginScreen() graphics2d.drawText(scripts, 'שלום abc', 10, 10, {size = 24, direction = 'auto', language = 'he', bold = true, italic = true, align = 'end', maxWidth = 300})"), "nil");
    EXPECT_NE(lua("graphics2d.measureText(nil, 'x', {direction = 'up'})").find("direction"), std::string::npos);
    EXPECT_NE(lua("graphics2d.newRichText('[p dir=up]x[/p]')").find("The dir of [p] must be auto, ltr or rtl."), std::string::npos);
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

    // An effect cannot change the text it runs on, and the text works again once the effect is gone.
    lua("graphics2d.registerTextEffect('rewrite', function(glyph) story.markup = 'other' end) story = graphics2d.newRichText('[rewrite]ab[/rewrite]')");
    EXPECT_NE(lua("story:layout()").find("A text effect cannot change or lay out the rich text it runs on."), std::string::npos);
    EXPECT_EQ(lua("story.markup = 'plain' return #story:layout().glyphs"), "5");

    // The index of a glyph counts every character inside the tag, spaces and icons included, from the one the tag starts with.
    lua("indices = {} graphics2d.registerTextEffect('probe', function(glyph) indices[#indices + 1] = glyph.index end) graphics2d.registerTextIcon('dot', assets.texture('images/coin.png'))");
    EXPECT_EQ(lua("graphics2d.newRichText('x[probe] a[icon=dot]b[/probe]'):layout() return table.concat(indices, ',')"), "2,4");

    // A new family may cluster the text differently, so the characters the tags start at are found again.
    lua(R"(indices = {} probed = graphics2d.newRichText('e\u{301}[probe]x[/probe]') probed:layout())");
    lua("probed.family = graphics.newGridFont(assets.texture('images/coin.png'), {characters = 'ex', cellWidth = 8, cellHeight = 8}) probed:layout()");
    EXPECT_EQ(lua("return table.concat(indices, ',') .. ' ' .. probed.characterCount"), "1,1 3");

    lua("graphics2d.registerTextIcon('coin', assets.texture('images/coin.png'), {source = {0, 0, 16, 16}, width = 20, height = 20})");
    EXPECT_EQ(lua("local layout = graphics2d.newRichText('pay [icon=coin] [icon=coin height=10]'):layout() return layout.images[1].rect.width .. ' ' .. layout.images[2].rect.width"), "20.0 10.0");
    EXPECT_NE(lua("graphics2d.registerTextIcon('bad', nil)").find("error"), std::string::npos);

    lua("family = graphics.newFontFamily({regular = font}) ui.addFont('story', family) ui.addFont('pixel', assets.font('fonts/pixel.fnt'))");
    EXPECT_EQ(lua("local story = graphics2d.newRichText('[font=serif]x[/font]', {fonts = {serif = family}}) return story.characterCount"), "1");
    EXPECT_NE(lua("ui.addFont('story', family)").find("already has a font named story"), std::string::npos);
}

} // namespace haylen
