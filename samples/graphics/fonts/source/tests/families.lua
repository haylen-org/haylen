-- Font families: rich text picks the face of each run from its style. Crimson Text has real bold, italic and bold italic faces and Space Mono for code, while Lilita One and the bitmap Haylen Pixel have one face, so their bold and italic are synthesized.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local fonts = require('fonts')
local sample = require('sample')

local Families = haylen.class('Families', sample.Test)

Families.hints = 'A distance field face grows its strokes for bold and leans its glyphs for italic, and a bitmap face draws a bold glyph twice a pixel apart. The captions come from "family:select", which tells which faces are real.'

local kMarkup = 'Regular [b]Bold[/b] [i]Italic[/i] [b][i]Bold italic[/i][/b] [code]mono()[/code]'
local kParagraph = 'The keeper wrote: [i]"The beacon went dark on the [b]third[/b] night."[/i] Call [code]light(beacon)[/code] before dawn.'
local kStyles = {{'bold', {bold = true}}, {'italic', {italic = true}}, {'bold italic', {bold = true, italic = true}}}

-- Rich texts of the three families, each with a caption that says which styles are real faces.
function Families:init(entry)
    Families.super.init(self, entry)
    self.rows = {}
    for _, spec in ipairs({{'crimson', 'Crimson Text with real faces and Space Mono', 44, 32}, {'lilita', 'Lilita One, one face', 44, 30}, {'pixel', 'Haylen Pixel, one bitmap face at four and three times its size', 32, 24}}) do
        local family = fonts.family(spec[1])
        local notes = {}
        for index, style in ipairs(kStyles) do
            local _, syntheticBold, syntheticItalic = family:select(style[2])
            notes[index] = style[1] .. ((syntheticBold or syntheticItalic) and ' synthesized' or ' real')
        end
        notes[#notes + 1] = family.mono and 'mono real' or 'mono uses the regular face'
        self.rows[#self.rows + 1] = {
            title = spec[2],
            notes = table.concat(notes, ', '),
            line = graphics2d.newRichText(kMarkup, {family = family, size = spec[3]}),
            paragraph = graphics2d.newRichText(kParagraph, {family = family, size = spec[4], maxWidth = 1500}),
        }
    end
end

function Families:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    graphics2d.beginScreen()
    local y = stage.y
    for _, row in ipairs(self.rows) do
        sample.caption(row.title, stage.x, y, {size = 26, color = '#FF8FB0FF'})
        sample.caption(row.notes, stage.x + 760, y + 4)
        row.line:draw(stage.x, y + 40)
        local _, height = row.line:size()
        row.paragraph:draw(stage.x, y + 50 + height)
        local _, paragraphHeight = row.paragraph:size()
        y = y + 90 + height + paragraphHeight
    end
end

return Families
