-- The component `ui.richText` with styles, colors, outlines, shadows and glows, links and hints, inline images and icons, lists and tables, animated effects and a dialogue with a typewriter reveal.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local RichText = haylen.class('RichText', Test)

RichText.styles = [==[
[b]Bold[/b], [i]italic[/i], [u]underline[/u], [s]strike[/s] and [code]code()[/code].
[color=#FF6A6A]Colors[/color], [bgcolor=#604C7DFF] backgrounds [/bgcolor], [size=150%]sizes[/size] and [alpha=0.5]alpha[/alpha].
[outline=3 color=#FF402000][color=gold]Outline[/color][/outline], [shadow=3,3 color=#C0000000 blur=2]shadow[/shadow] and [glow=6 color=#FF8000]glow[/glow].
[font=lilitaOne]A font the parchment theme registered[/font] next to the body font.
Twelve coins [img=interface/icons/coin.png height=28] or press [icon=confirm] to open the chest [icon=coin].
Read the [url=manual]manual[/url], the [url=map]map[/url] or the [hint=Frequently asked questions]FAQ[/hint].]==]

RichText.blocks = [==[
[ul]
Collect [color=gold]12 wood[/color]
Keep the [wave amp=6]fire[/wave] burning
[/ul]
[ol type=a]
Light the beacon
Wait for the ship
[/ol]
[hr width=80%]
[table=2][cell bg=#40000000 padding=6]Wood[/cell][cell padding=6]12[/cell][cell bg=#40000000 padding=6]Stone[/cell][cell padding=6]4[/cell][/table]]==]

RichText.effects = '[wave]Wave[/wave]  [shake]Shake[/shake]  [tornado radius=4]Tornado[/tornado]  [rainbow]Rainbow[/rainbow]  [pulse]Pulse[/pulse]  [fade length=6]Fading out[/fade]'

RichText.dialogue = {
    '[b]Keeper:[/b] You made it through the storm.[pause=0.5] Few do.',
    '[b]Keeper:[/b] The beacon went dark three nights ago.[pause=0.4] [shake]Something[/shake] is out there.',
    '[b]Keeper:[/b] Take this [img=interface/icons/key.png height=28] and [speed=0.4]be careful[/speed]...[pause=0.4] [rainbow]good luck![/rainbow]',
}

function RichText:init(entry)
    RichText.super.init(self, entry)
    self.line = 1
end

-- The parchment theme registers the font that the markup names, and the icons that `[icon=name]` shows are registered for the whole app.
function RichText:enter()
    ui.loadTheme('interface/themes/parchment.json', 'light')
    graphics2d.registerTextIcon('coin', assets.texture('interface/icons/coin.png'))
    graphics2d.registerTextIcon('confirm', assets.texture('interface/icons/star.png'))
    self:frame{
        hint = 'Click, tap or focus a link and press Enter to follow it, and rest the pointer on FAQ for its hint. Next line reveals the next line of the dialogue, and Skip shows the rest at once.',
        focus = 'next',
        content = {self:columns()},
    }
end

function RichText:report(text)
    self:set('status', {text = text})
end

function RichText:columns()
    return layout.columns{
        ui.column{grow = 3, gap = 24,
            layout.section('Styles, links, hints, images and icons', {
                ui.richText{id = 'styles', text = RichText.styles, onLink = function(event)
                    self:report('Followed the link "' .. event.link .. '".')
                end, onLinkHover = function(event)
                    self:report(string.format('%s the link "%s".', event.hovered and 'Hovered' or 'Left', event.link))
                end},
            }),
            layout.section('Effects', {ui.richText{text = RichText.effects, font = 'heading'}}),
            layout.section('Fill alignment', {
                ui.richText{text = 'Fill stretches the spaces of every wrapped line to both edges, except the last line of the paragraph, which stays at the start like the text of a book.', textAlign = 'fill'},
            }),
        },
        ui.column{grow = 2, gap = 24,
            layout.section('Lists, rules and tables', {ui.richText{text = RichText.blocks}}),
            layout.section('Typewriter reveal', {
                ui.richText{id = 'dialogue', text = RichText.dialogue[1], revealSpeed = 30, font = 'heading'},
                ui.row{gap = 12,
                    ui.button{id = 'next', text = 'Next line', variant = 'primary', onClick = function(event)
                        self.line = self.line % #RichText.dialogue + 1
                        event.gui:set('dialogue', {text = RichText.dialogue[self.line]})
                        self:report('The dialogue shows line ' .. self.line .. '.')
                    end},
                    ui.button{text = 'Skip', onClick = function(event)
                        event.gui:set('dialogue', {visibleCharacters = -1})
                    end},
                },
            }),
        },
    }
end

return RichText
