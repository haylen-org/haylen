-- Fallback fonts: the family draws each character with the first font that has it, the face of its style, then M PLUS 1p for Japanese and Chinese, then Noto Sans Symbols 2, and the face again with its missing glyph box when none has it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Fallback = haylen.class('Fallback', TextTest)

Fallback.code = [[
local cjk = assets.font('fonts/mplus-1p-regular.ttf')
local symbols = assets.font('fonts/noto-sans-symbols-2-regular.ttf')
local family = graphics.newFontFamily({regular = serif, fallbacks = {cjk, symbols}})
local line = graphics2d.newRichText('日本語のテキスト ★ ♚', {family = family, size = 34})
local font, syntheticBold = family:resolve('★', {bold = true})]]

Fallback.lines = {
    {'Japanese', '日本語のテキスト、ひらがなとカタカナと漢字。'},
    {'Chinese', '我在學習繁體中文。春夏秋冬。'},
    {'Cards, stars and arrows', '♠ ♥ ♦ ♣    ★ ☆    ✓ ✗    ← ↑ → ↓    ♪'},
    {'Weather, chess and travel', '☀ ☂ ☃    ♚ ♛ ♜ ♞    ✈ ☎ ✉ ✂    ⌛ ⏳'},
    {'Bold, italic and mono', '[b]Bold 太字 ★[/b]    [i]Italic 斜体 ☀[/i]    [code]mono ♥[/code]'},
}
Fallback.probes = {'A', 'あ', '學', '★', '♚', '✈', '⚓'}
Fallback.textX = 340
Fallback.textSize = 34
Fallback.tableWidth = 640

function Fallback:init(entry)
    Fallback.super.init(self, entry)
    local family = fonts.family('crimson')
    local crimson, cjk, symbols = fonts.load('crimson'), fonts.load('cjk'), fonts.load('symbols')
    local size = Fallback.textSize
    self.lines = {}
    for index, line in ipairs(Fallback.lines) do
        self.lines[index] = Fallback.labelled(line[1], graphics2d.newRichText(line[2], {family = family, size = size, color = Test.ink}))
    end
    self.alone = Fallback.labelled('Without fallbacks', graphics2d.newRichText('日本 ★ ♥', {family = graphics.newFontFamily({regular = crimson}), size = size, color = Test.ink}))
    self.missing = Fallback.labelled('In no font', graphics2d.newRichText('⚓', {family = family, size = size, color = Test.ink}))

    local candidates = {{crimson, 'Crimson Text'}, {cjk, 'M PLUS 1p'}, {symbols, 'Noto Sans Symbols 2'}}
    self.probes = {}
    for index, character in ipairs(Fallback.probes) do
        local drawn
        for _, candidate in ipairs(candidates) do
            if not drawn and candidate[1]:hasGlyph(character) then
                drawn = candidate[2]
            end
        end
        local _, syntheticBold = family:resolve(character, {bold = true})
        local note = drawn and string.format('%s, %s', drawn, syntheticBold and 'bold synthesized' or 'with a real bold face') or 'No font has it, so the missing box shows'
        self.probes[index] = {glyph = graphics2d.newRichText(character, {family = family, size = size, color = Test.ink}), note = note}
    end
end

function Fallback:enter()
    self:frame{code = Fallback.code, hint = 'M PLUS 1p comes first among the fallbacks, so it draws the symbols it has and Noto Sans Symbols 2 draws the rest. A fallback font synthesizes the bold and italic its run asks for, as "family:resolve" reports in the table.'}
end

-- A labelled line keeps the height and the first baseline of its text, which places its label on that baseline.
function Fallback.labelled(label, text)
    local _, height = text:size()
    return {label = label, text = text, height = height, baseline = text:frame().lines[1].baseline - graphics2d.defaultFont():ascent(26)}
end

function Fallback:row(line, y, pitch)
    local top = y + (pitch - line.height) / 2
    TextTest.label(line.label, 0, top + line.baseline)
    line.text:draw(Fallback.textX, top)
end

function Fallback:draw(area)
    local layout = self.layout
    local slots = #self.lines + 3
    local pitch = layout.height / slots
    local right = layout.width - Fallback.tableWidth

    local y = 0
    for _, line in ipairs(self.lines) do
        self:row(line, y, pitch)
        y = y + pitch
    end
    graphics2d.drawLine(0, y + pitch / 2, right - 60, y + pitch / 2, 2, Test.line)
    y = y + pitch
    self:row(self.alone, y, pitch)
    self:row(self.missing, y + pitch, pitch)

    -- The table keeps the glyph in one column and the font that draws it in the next, one row per slot of the lines beside it.
    TextTest.label('Which font draws each character', right, (pitch - 34) / 2, {size = 28, color = Test.accent})
    for index, probe in ipairs(self.probes) do
        local top = index * pitch
        local width, height = probe.glyph:size()
        probe.glyph:draw(right + 24 - width / 2, top + (pitch - height) / 2)
        TextTest.label(probe.note, right + 72, top + (pitch - 30) / 2)
    end
end

return Fallback
