-- Font families: rich text picks the face of each run from its style. Crimson Text has real bold, italic and bold italic faces and Space Mono for code, while Lilita One and the bitmap Haylen Pixel have one face, so their bold and italic are synthesized.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Families = haylen.class('Families', TextTest)

Families.markup = 'Regular [b]Bold[/b] [i]Italic[/i] [b][i]Bold italic[/i][/b] [code]mono()[/code]'
Families.paragraph = 'The keeper wrote: [i]"The beacon went dark on the [b]third[/b] night."[/i] Call [code]light(beacon)[/code] before dawn.'
Families.styles = {{'Bold', {bold = true}}, {'italic', {italic = true}}, {'bold italic', {bold = true, italic = true}}}
Families.specs = {
    {'crimson', 'Crimson Text with real faces and Space Mono', 44, 32},
    {'lilita', 'Lilita One, one face', 44, 30},
    {'pixel', 'Haylen Pixel, one bitmap face at four and three times its size', 32, 24},
}

-- Rich texts of the three families, each with a caption that says which styles are real faces.
function Families:init(entry)
    Families.super.init(self, entry)
    self.rows = {}
    for _, spec in ipairs(Families.specs) do
        local family = fonts.family(spec[1])
        local notes = {}
        for index, style in ipairs(Families.styles) do
            local _, syntheticBold, syntheticItalic = family:select(style[2])
            notes[index] = style[1] .. ((syntheticBold or syntheticItalic) and ' synthesized' or ' real')
        end
        notes[#notes + 1] = family.mono and 'mono real' or 'mono from the regular face'
        self.rows[#self.rows + 1] = {
            title = spec[2],
            notes = table.concat(notes, ', '),
            line = graphics2d.newRichText(Families.markup, {family = family, size = spec[3]}),
            paragraph = graphics2d.newRichText(Families.paragraph, {family = family, size = spec[4], maxWidth = 1500}),
        }
    end
end

function Families:enter()
    self:frame{hint = 'A distance field face grows its strokes for bold and leans its glyphs for italic, and a bitmap face draws a bold glyph twice a pixel apart. The captions come from "family:select", which tells which faces are real.'}
end

function Families:draw(area)
    local y = 0
    for _, row in ipairs(self.rows) do
        Test.caption(row.title, 0, y, {size = 26, color = Test.accent})
        Test.caption(row.notes, 760, y + 4, {size = 22})
        row.line:draw(0, y + 40)
        local _, height = row.line:size()
        row.paragraph:draw(0, y + 50 + height)
        local _, paragraphHeight = row.paragraph:size()
        y = y + 90 + height + paragraphHeight
    end
end

return Families
