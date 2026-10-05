-- Effects and typewriter: the built-in effects animate their glyphs from the time alone, with the attributes of their tags, and a typewriter reveals a dialogue with `[pause]` and `[speed]` tags.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Typewriter = haylen.class('Typewriter', TextTest)

Typewriter.effects = {
    {'[wave]', '[wave]The sea rolls[/wave]'},
    {'[wave amp=8 freq=2]', '[wave amp=8 freq=2]A gentle swell[/wave]'},
    {'[shake rate=20 level=5]', '[shake rate=20 level=5]Thunder![/shake]'},
    {'[tornado radius=10 freq=1]', '[tornado radius=10 freq=1]A whirlpool[/tornado]'},
    {'[fade start=4 length=12]', '[fade start=4 length=12]Fading into the fog[/fade]'},
    {'[rainbow freq=1 sat=0.8 val=0.9]', '[rainbow freq=1 sat=0.8 val=0.9]Northern lights[/rainbow]'},
    {'[pulse freq=2 color=#FFFF8A00 ease=-2]', '[pulse freq=2 color=#FFFF8A00 ease=-2]The beacon burns[/pulse]'},
    {'[wave][rainbow] nested', '[wave amp=12][rainbow]Treasure![/rainbow][/wave]'},
}
Typewriter.dialogue = {
    '[b]Keeper:[/b] You made it through the storm.[pause=0.6] Few ships do.',
    '[b]Keeper:[/b] The beacon went dark three nights ago.[pause=0.5] [shake]Something[/shake] is out there.',
    '[b]Keeper:[/b] Take this key [img=text/star.png height=34] and [speed=0.3]be careful...[/speed][pause=0.4] [rainbow]good luck![/rainbow]',
}

function Typewriter:init(entry)
    Typewriter.super.init(self, entry)
    self.line = 1
    self.speed = 30
    self.family = fonts.family('crimson')
    self.samples = {}
    for index, effect in ipairs(Typewriter.effects) do
        self.samples[index] = {label = effect[1], text = graphics2d.newRichText(effect[2], {family = self.family, size = 44})}
    end
    self:say()
end

-- Starts the current line of the dialogue with the reveal speed of the slider.
function Typewriter:say()
    self.spoken = graphics2d.newRichText(Typewriter.dialogue[self.line], {family = self.family, size = 40, maxWidth = 1300, revealSpeed = self.speed})
end

function Typewriter:enter()
    self:frame{
        hint = 'Next line reveals the next line of the dialogue, Skip shows the rest at once and Replay starts the line over. The speed slider sets the characters per second, and "[speed]" tags change it inside a line.',
        controls = {
            ui.button{id = 'next', text = 'Next line', variant = 'primary', align = 'stretch', onClick = function()
                self.line = self.line % #Typewriter.dialogue + 1
                self:say()
            end},
            ui.row{gap = 12,
                ui.button{id = 'skip', text = 'Skip', grow = 1, onClick = function() self.spoken.visibleCharacters = -1 end},
                ui.button{id = 'replay', text = 'Replay', grow = 1, onClick = function() self:say() end},
            },
            ui.formField{label = 'Characters per second', ui.slider{id = 'speed', min = 5, max = 80, step = 5, value = self.speed, showValue = true, decimals = 0, onChange = function(event)
                self.speed = event.value
                self:say()
            end}},
            ui.progress{id = 'revealed', text = 'Revealed'},
        },
        focus = 'next',
    }
end

function Typewriter:update(dt)
    Typewriter.super.update(self, dt)
    for _, sample in ipairs(self.samples) do
        sample.text:update(dt)
    end
    local spoken = self.spoken
    spoken:update(dt)
    self:set('revealed', {value = spoken.visibleRatio})
    self:status(string.format('Line %d   Characters %d of %d   %s', self.line, spoken.revealing and spoken.visibleCharacters or spoken.characterCount, spoken.characterCount, spoken.revealing and 'Revealing' or 'Done'))
end

function Typewriter:draw(area)
    local layout = self.layout
    local y = 0
    for _, sample in ipairs(self.samples) do
        Test.caption(sample.label, 0, y + 14, {size = 22})
        sample.text:draw(520, y)
        y = y + 64
    end
    local box = {0, layout.height - 170, layout.width, 170}
    graphics2d.drawRect(box, '#E0101828')
    graphics2d.drawRectOutline(box, 3, Test.accent)
    self.spoken:draw(box[1] + 30, box[2] + 24)
end

return Typewriter
