-- Fallback fonts: the family draws each character with the first font that has it, the face of its style, then M PLUS 1p for Japanese and Chinese, then Noto Sans Symbols 2, and the face again with its missing glyph box when none has it.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local fonts = require('fonts')
local sample = require('sample')

local Fallback = haylen.class('Fallback', sample.Test)

Fallback.hints = 'M PLUS 1p comes first among the fallbacks, so it draws the symbols it has and Noto Sans Symbols 2 draws the rest. A fallback font synthesizes the bold and italic its run asks for, as "family:resolve" reports on the right.'

local kLines = {
    {'Japanese', '日本語のテキスト、ひらがなとカタカナと漢字。'},
    {'Chinese', '我在學習繁體中文。春夏秋冬。'},
    {'M PLUS 1p', 'Cards ♠ ♥ ♦ ♣   Stars ★ ☆   Checks ✓ ✗   Arrows ← ↑ → ↓   Music ♪'},
    {'Symbols 2', 'Weather ☀ ☂ ☃   Chess ♚ ♛ ♜ ♞   Travel ✈ ☎ ✉ ✂   Time ⌛ ⏳'},
    {'Styles', '[b]Bold 太字 ★[/b]   [i]Italic 斜体 ☀[/i]   [code]mono ♥[/code]'},
}
local kProbe = {'A', 'あ', '學', '★', '♚', '✈', '⚓'}

function Fallback:init(entry)
    Fallback.super.init(self, entry)
    local family = fonts.family('crimson')
    self.lines = {}
    for index, line in ipairs(kLines) do
        self.lines[index] = {label = line[1], text = graphics2d.newRichText(line[2], {family = family, size = 34})}
    end
    self.alone = graphics2d.newRichText('Without fallbacks: 日本 ★ ♥', {family = graphics.newFontFamily({regular = fonts.get('crimson')}), size = 34})
    self.missing = graphics2d.newRichText('In no font: ⚓', {family = family, size = 34})

    local candidates = {{fonts.get('crimson'), 'Crimson Text'}, {fonts.get('cjk'), 'M PLUS 1p'}, {fonts.get('symbols'), 'Symbols 2'}}
    local notes = {}
    for index, character in ipairs(kProbe) do
        local drawn = 'missing glyph box'
        for _, candidate in ipairs(candidates) do
            if drawn == 'missing glyph box' and candidate[1]:hasGlyph(character) then
                drawn = candidate[2]
            end
        end
        local _, syntheticBold = family:resolve(character, {bold = true})
        notes[index] = string.format('%s  %s, bold %s', character, drawn, syntheticBold and 'synthesized' or 'real')
    end
    self.notes = table.concat(notes, '\n')
end

function Fallback:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    graphics2d.beginScreen()
    local y = stage.y
    for _, line in ipairs(self.lines) do
        sample.caption(line.label, stage.x, y + 14)
        line.text:draw(stage.x + 180, y)
        y = y + 76
    end
    graphics2d.drawLine(stage.x, y, stage.x + 1100, y, 2, sample.guide)
    self.alone:draw(stage.x + 180, y + 16)
    self.missing:draw(stage.x + 180, y + 92)
    sample.caption('the first font that has it, and family:resolve for bold', stage.x + 1260, stage.y, {size = 26, color = '#FF8FB0FF'})
    graphics2d.drawRichText(self.notes, stage.x + 1260, stage.y + 44, {family = fonts.family('crimson'), size = 26})
end

return Fallback
