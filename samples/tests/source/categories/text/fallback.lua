-- Fallback fonts: the family draws each character with the first font that has it, the face of its style, then M PLUS 1p for Japanese and Chinese, then Noto Sans Symbols 2, and the face again with its missing glyph box when none has it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Fallback = haylen.class('Fallback', TextTest)

Fallback.lines = {
    {'Japanese', '日本語のテキスト、ひらがなとカタカナと漢字。'},
    {'Chinese', '我在學習繁體中文。春夏秋冬。'},
    {'M PLUS 1p', 'Cards ♠ ♥ ♦ ♣   Stars ★ ☆   Checks ✓ ✗   Arrows ← ↑ → ↓   Music ♪'},
    {'Symbols 2', 'Weather ☀ ☂ ☃   Chess ♚ ♛ ♜ ♞   Travel ✈ ☎ ✉ ✂   Time ⌛ ⏳'},
    {'Styles', '[b]Bold 太字 ★[/b]   [i]Italic 斜体 ☀[/i]   [code]mono ♥[/code]'},
}
Fallback.probes = {'A', 'あ', '學', '★', '♚', '✈', '⚓'}

function Fallback:init(entry)
    Fallback.super.init(self, entry)
    local family = fonts.family('crimson')
    local crimson, cjk, symbols = fonts.load('crimson'), fonts.load('cjk'), fonts.load('symbols')
    self.lines = {}
    for index, line in ipairs(Fallback.lines) do
        self.lines[index] = {label = line[1], text = graphics2d.newRichText(line[2], {family = family, size = 34})}
    end
    self.alone = graphics2d.newRichText('Without fallbacks: 日本 ★ ♥', {family = graphics.newFontFamily({regular = crimson}), size = 34})
    self.missing = graphics2d.newRichText('In no font: ⚓', {family = family, size = 34})

    local candidates = {{crimson, 'Crimson Text'}, {cjk, 'M PLUS 1p'}, {symbols, 'Symbols 2'}}
    local notes = {}
    for index, character in ipairs(Fallback.probes) do
        local drawn = 'missing glyph box'
        for _, candidate in ipairs(candidates) do
            if drawn == 'missing glyph box' and candidate[1]:hasGlyph(character) then
                drawn = candidate[2]
            end
        end
        local _, syntheticBold = family:resolve(character, {bold = true})
        notes[index] = string.format('%s  %s, bold %s', character, drawn, syntheticBold and 'synthesized' or 'real')
    end
    self.notes = graphics2d.newRichText(table.concat(notes, '\n'), {family = family, size = 26})
end

function Fallback:enter()
    self:frame{hint = 'M PLUS 1p comes first among the fallbacks, so it draws the symbols it has and Noto Sans Symbols 2 draws the rest. A fallback font synthesizes the bold and italic its run asks for, as "family:resolve" reports on the right.'}
end

function Fallback:draw(area)
    local y = 0
    for _, line in ipairs(self.lines) do
        Test.caption(line.label, 0, y + 14, {size = 22})
        line.text:draw(180, y)
        y = y + 76
    end
    graphics2d.drawLine(0, y, 1100, y, 2, Test.line)
    self.alone:draw(180, y + 16)
    self.missing:draw(180, y + 92)
    Test.caption('The first font that has it, and "family:resolve" for bold', 1200, 0, {size = 24, color = Test.accent})
    self.notes:draw(1200, 44)
end

return Fallback
