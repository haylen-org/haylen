-- Font families: rich text picks the face of each run from its style. Crimson Text has real bold, italic and bold italic faces and Space Mono for code, while Lilita One and the bitmap Haylen Pixel have one face, so their bold and italic are synthesized. Plain text underlines and strikes itself through its style too.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Families = haylen.class('Families', TextTest)

Families.code = [[
local family = graphics.newFontFamily({regular = serif, bold = serifBold, italic = serifItalic, mono = mono})
local line = graphics2d.newRichText('Regular [b]Bold[/b] [i]Italic[/i] [code]mono()[/code]', {family = family, size = 44})
local face, syntheticBold, syntheticItalic = family:select({italic = true})
graphics2d.drawText(family, 'Read the map carefully', x, y, {size = 36, underline = true})
graphics2d.drawText(family, 'Old price 120 coins', x, y, {size = 36, strike = true})]]

Families.markup = 'Regular [b]Bold[/b] [i]Italic[/i] [b][i]Bold italic[/i][/b] [code]mono()[/code]'
Families.paragraph = 'The keeper wrote: [i]"The beacon went dark on the [b]third[/b] night."[/i] Call [code]light(beacon)[/code] before dawn.'
Families.styles = {{'bold', {bold = true}}, {'italic', {italic = true}}, {'bold italic', {bold = true, italic = true}}}
Families.specs = {
    {'crimson', 'Crimson Text with real faces and Space Mono', 44, 32},
    {'lilita', 'Lilita One, one face', 44, 30},
    {'pixel', 'Haylen Pixel, one bitmap face', 4, 3},
}

function Families:init(entry)
    Families.super.init(self, entry)
    self.pixel = fonts.load('pixel')
    self.serif = fonts.family('crimson')
    self.rows = {}
    for index, spec in ipairs(Families.specs) do
        local family = fonts.family(spec[1])
        self.rows[index] = {family = family, title = spec[2], notes = Families.describe(family), lineSize = spec[3], paragraphSize = spec[4]}
    end
end

-- Tells which styles of a family are real faces and which are synthesized, from what "family:select" reports.
function Families.describe(family)
    local real, synthesized = {}, {}
    for _, style in ipairs(Families.styles) do
        local _, syntheticBold, syntheticItalic = family:select(style[2])
        local list = (syntheticBold or syntheticItalic) and synthesized or real
        list[#list + 1] = style[1]
    end
    if #synthesized == 0 then
        return 'Real faces for ' .. table.concat(real, ', ') .. (family.mono and ' and mono' or '')
    end
    local text = Families.sentence(synthesized) .. ' synthesized'
    return text .. (family.mono and ', and a real mono face' or ', and mono from the regular face')
end

function Families.sentence(words)
    local text = words[1]:sub(1, 1):upper() .. words[1]:sub(2)
    for index = 2, #words do
        text = text .. (index == #words and ' and ' or ', ') .. words[index]
    end
    return text
end

function Families:enter()
    self:frame{code = Families.code, hint = 'A distance field face grows its strokes for bold and leans its glyphs for italic, and a bitmap face draws a bold glyph twice a pixel apart. The notes beside each title come from "family:select", which tells which faces are real.'}
end

-- Lays the rows out again whenever the page changes, since the bitmap face takes the sizes at which its pixels stay whole on this screen.
function Families:resize(area)
    Families.super.resize(self, area)
    local width = self.layout.width
    for index, row in ipairs(self.rows) do
        local lineSize, paragraphSize = row.lineSize, row.paragraphSize
        if index == #self.rows then
            lineSize, paragraphSize = self:pixelSize(self.pixel, lineSize), self:pixelSize(self.pixel, paragraphSize)
        end
        row.line = graphics2d.newRichText(Families.markup, {family = row.family, size = lineSize, color = Test.ink})
        row.text = graphics2d.newRichText(Families.paragraph, {family = row.family, size = paragraphSize, maxWidth = width, color = Test.ink})
    end
end

function Families:draw(area)
    local layout = self.layout
    local heights, total = {}, 0
    for index, row in ipairs(self.rows) do
        local _, lineHeight = row.line:size()
        local _, textHeight = row.text:size()
        heights[index] = 44 + lineHeight + 10 + textHeight
        total = total + heights[index]
    end
    local decorations = 44 + 50
    local gap = (layout.height - total - decorations) / #self.rows

    local y = 0
    for index, row in ipairs(self.rows) do
        TextTest.label(row.title, 0, y, {size = 28, color = Test.accent})
        TextTest.label(row.notes, layout.width, y + 2, {anchor = {1, 0}})
        local x, top = 0, y + 44
        if index == #self.rows then
            x, top = self:onPixel(0, top)
        end
        row.line:draw(x, top)
        local _, lineHeight = row.line:size()
        row.text:draw(x, top + lineHeight + 10)
        y = y + heights[index] + gap
    end

    TextTest.label('Plain text with the options "underline" and "strike"', 0, y, {size = 28, color = Test.accent})
    local serif, third = self.serif, layout.width / 3
    graphics2d.drawText(serif, 'Read the map carefully', 0, y + 44, {size = 36, underline = true, color = Test.ink})
    graphics2d.drawText(serif, 'Old price 120 coins', third, y + 44, {size = 36, strike = true, color = Test.muted})
    graphics2d.drawText(serif, 'Underlined and struck at once', third * 2, y + 44, {size = 36, underline = true, strike = true, color = Test.warm})
end

return Families
