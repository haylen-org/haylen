-- Rich text tags: every tag of the markup, inline ones on the left and blocks on the right, drawn by `graphics2d.newRichText` with the Crimson Text family, two more families for `[font]` and registered icons for `[icon]`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')

local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local RichText = haylen.class('RichText', TextTest)

RichText.prompts = {'south', 'east', 'west', 'north'}
RichText.blocksLeft = 960

RichText.inline = [==[
[b]b[/b] bold, [i]i[/i] italic, [u]u[/u] underline, [s]s[/s] strike and [code]code[/code] in the mono face.
[color=#FF6A6A]color[/color] by value or [color=gold]by name[/color], [bgcolor=#804C7DFF] bgcolor [/bgcolor] and [alpha=0.4]alpha 0.4[/alpha].
[font=pixel]font=pixel[/font] and [font=lilita]font=lilita[/font] name other families.
[size=22]size=22[/size], [size=150%]size=150%[/size] and back.
[outline=3 color=#FF8A1E1E]outline=3[/outline], [shadow=3,4 color=#C0000000 blur=2]shadow=3,4 blur=2[/shadow] and [glow=8 color=#FF40C4FF]glow=8[/glow].
[url=manual]url=manual[/url], [url]https://haylen.dev[/url] and a [hint=Hints show as tooltips in the UI]hint[/hint].
[img=text/coin.png height=32] [img=text/heart.png width=40] [img=text/star.png height=24 valign=baseline] images, [icon=south] [icon=east] [icon=west] [icon=north color=#FFFFD0D0] icons.
[lb]b[rb] writes brackets, and [lb]br[rb] breaks[br]the line inside one paragraph.]==]

RichText.blocks = [==[
[center][size=130%][b]A centered title[/b][/size][/center]
[right]A paragraph at the right.[/right]
[p align=fill indent=1]A filled and indented paragraph stretches the spaces of its wrapped lines to both of its edges.[/p]
[ul bullet=*]
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
[hr width=70% height=3 color=#FF8FB0FF]
[table=3][cell bg=#FF2C3147 padding=6][b]Item[/b][/cell][cell bg=#FF2C3147 padding=6][b]Count[/b][/cell][cell bg=#FF2C3147 padding=6][b]Where[/b][/cell][cell border=#FF525A7A padding=6]Wood[/cell][cell border=#FF525A7A padding=6]12[/cell][cell border=#FF525A7A padding=6]By the fire[/cell][cell border=#FF525A7A padding=6]Stone[/cell][cell border=#FF525A7A padding=6]4[/cell][cell border=#FF525A7A padding=6]In the cave[/cell][/table]
[dropcap size=96 color=gold margin=6]O[/dropcap]nce the storm was over, the keeper climbed down to the shore and counted the barrels the waves had left on the sand.]==]

function RichText:init(entry)
    RichText.super.init(self, entry)
    local options = {family = fonts.family('crimson'), size = 30, maxWidth = 880, fonts = {pixel = fonts.family('pixel'), lilita = fonts.family('lilita')}}
    self.inline = graphics2d.newRichText(RichText.inline, options)
    options.maxWidth = 820
    self.blocks = graphics2d.newRichText(RichText.blocks, options)
end

-- Registers the gamepad prompts that `[icon=name]` shows, cut from one image, before the frame mounts.
function RichText:enter()
    local prompts = assets.texture('text/prompts.png', {filter = 'linear'})
    for index, name in ipairs(RichText.prompts) do
        graphics2d.registerTextIcon(name, prompts, {source = {(index - 1) * 32, 0, 32, 32}})
    end
    self:frame{hint = 'Move the pointer over a link or the hint: "text:linkAt" and "text:hintAt" report what lies under it in the status line. In a UI document the same links take the focus and the hint shows as a tooltip.'}
end

function RichText:update(dt)
    RichText.super.update(self, dt)
    self.inline:update(dt)
    self.blocks:update(dt)
    if not self.stage then
        return
    end
    local x, y = self:toStage(input.mousePosition())
    local link, hint = self.inline:linkAt(x, y), self.inline:hintAt(x, y)
    self:status(link and ('Link "' .. link .. '"') or hint and ('Hint: ' .. hint) or 'The pointer is over no link or hint')
end

function RichText:draw(area)
    self.inline:draw(0, 0)
    self.blocks:draw(RichText.blocksLeft, 0)
end

return RichText
