-- Rich text tags: every tag of the markup, inline ones in the first column, paragraph blocks in the second and lists, rules and tables in the third, drawn by `graphics2d.newRichText` with the Crimson Text family, two more families for `[font]` and registered icons for `[icon]`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local RichText = haylen.class('RichText', TextTest)

RichText.code = [[
graphics2d.registerTextIcon('south', prompts, {source = {0, 0, 64, 64}})
local options = {family = family, size = 30, maxWidth = 760, fonts = {pixel = pixelFamily}}
local text = graphics2d.newRichText('[b]Bold[/b], [color=gold]gold[/color] and [icon=south]', options)
text:draw(x, y)
local link, hint = text:linkAt(x, y), text:hintAt(x, y)]]

RichText.prompts = {'south', 'east', 'west', 'north'}
RichText.columns = {760, 520, 480}

RichText.inline = [==[
[b]b[/b] bold, [i]i[/i] italic, [u]u[/u] underline, [s]s[/s] strike and [code]code[/code] in the mono face.
[color=#FF6A6A]color[/color] by value or [color=gold]by name[/color], [bgcolor=#804C7DFF] bgcolor [/bgcolor] and [alpha=0.4]alpha 0.4[/alpha].
[font=pixel]font=pixel[/font] and [font=lilita]font=lilita[/font] name other families.
[size=22]size=22[/size], [size=150%]size=150%[/size] and back.
[outline=3 color=#FF8A1E1E]outline=3[/outline], [shadow=3,4 color=#C0000000 blur=2]shadow=3,4 blur=2[/shadow] and [glow=8 color=#FF40C4FF]glow=8[/glow].
[url=manual]url=manual[/url], [url]https://haylen.dev[/url] and a [hint=Hints show as tooltips in the UI]hint[/hint].
[img=text/coin.png height=32] [img=text/heart.png width=40] [img=text/star.png height=24 valign=baseline] images and [icon=south] [icon=east] [icon=west] [icon=north color=#FFFFD0D0] icons.
[lb]b[rb] writes brackets, and [lb]br[rb] breaks[br]the line inside one paragraph.]==]

RichText.paragraphs = [==[
[center][size=130%][b]A centered title[/b][/size][/center]
[right]A paragraph at the right.[/right]

[p align=fill indent=1]A filled and indented paragraph stretches the spaces of its wrapped lines to both of its edges.[/p]

[dropcap size=110 color=gold margin=10]O[/dropcap]nce the storm was over, the keeper climbed down to the shore and counted the barrels the waves had left on the sand.]==]

RichText.lists = [==[
[ul bullet=★]
A list with star bullets
[ol type=I]
Roman one
Roman two
[/ol]
[/ul]
[ol type=a]
Letters count the items
of an ordered list
[/ol]
[hr width=80% height=3 color=#FF8FB0FF]
[table=3][cell bg=#FF2C3147 border=#FF525A7A padding=10][b]Item[/b][/cell][cell bg=#FF2C3147 border=#FF525A7A padding=10][b]Count[/b][/cell][cell bg=#FF2C3147 border=#FF525A7A padding=10][b]Where[/b][/cell][cell border=#FF525A7A padding=10]Wood[/cell][cell border=#FF525A7A padding=10]12[/cell][cell border=#FF525A7A padding=10]By the fire[/cell][cell border=#FF525A7A padding=10]Stone[/cell][cell border=#FF525A7A padding=10]4[/cell][cell border=#FF525A7A padding=10]In the cave[/cell][/table]]==]

function RichText:init(entry)
    RichText.super.init(self, entry)
    self.options = {family = fonts.family('crimson'), size = 30, color = Test.ink, fonts = {pixel = fonts.family('pixel'), lilita = fonts.family('lilita')}}
end

-- Registers the gamepad prompts that `[icon=name]` shows, cut from one image, before the rich texts lay out.
function RichText:enter()
    local prompts = assets.texture('text/prompts.png', {filter = 'linear'})
    for index, name in ipairs(RichText.prompts) do
        graphics2d.registerTextIcon(name, prompts, {source = {(index - 1) * 64, 0, 64, 64}})
    end
    self.texts = {}
    for index, markup in ipairs({RichText.inline, RichText.paragraphs, RichText.lists}) do
        self.options.maxWidth = RichText.columns[index]
        self.texts[index] = graphics2d.newRichText(markup, self.options)
    end
    self:frame{code = RichText.code, page = {1880, 560}, hint = 'Move the pointer over a link or the hint: "text:linkAt" and "text:hintAt" report what lies under it in the status line. In a GUI the same links take the focus and the hint shows as a tooltip.'}
end

-- The columns stand apart by the room the page has past them.
function RichText:columnX(index)
    local widths = RichText.columns
    local gap = (self.layout.width - widths[1] - widths[2] - widths[3]) / 2
    local x = 0
    for column = 1, index - 1 do
        x = x + widths[column] + gap
    end
    return x
end

function RichText:update(dt)
    RichText.super.update(self, dt)
    for _, text in ipairs(self.texts) do
        text:update(dt)
    end
    if not self.stage then
        return
    end
    local x, y = self:toStage(input.mousePosition())
    local inline = self.texts[1]
    local link, hint = inline:linkAt(x, y), inline:hintAt(x, y)
    self:status(link and ('The pointer is over the link "' .. link .. '".') or hint and ('The pointer is over a hint that reads "' .. hint .. '".') or 'The pointer is over no link or hint.')
end

function RichText:draw(area)
    for index, text in ipairs(self.texts) do
        text:draw(self:columnX(index), 0)
    end
end

return RichText
