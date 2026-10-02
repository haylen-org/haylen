-- Effects and typewriter: the built-in effects animate their glyphs from the time alone, with the attributes of their tags, and a typewriter reveals a dialogue with `[pause]` and `[speed]` tags.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local fonts = require('fonts')
local sample = require('sample')

local Typewriter = haylen.class('Typewriter', sample.Test)

Typewriter.hints = 'Next line reveals the next line of the dialogue, Skip shows the rest at once and Replay starts the line over. The speed slider sets the characters per second, and "[speed]" tags change it inside a line.'
Typewriter.focus = 'next'

local kEffects = {
    {'[wave]', '[wave]The sea rolls[/wave]'},
    {'[wave amp=8 freq=2]', '[wave amp=8 freq=2]A gentle swell[/wave]'},
    {'[shake rate=20 level=5]', '[shake rate=20 level=5]Thunder![/shake]'},
    {'[tornado radius=10 freq=1]', '[tornado radius=10 freq=1]A whirlpool[/tornado]'},
    {'[fade start=4 length=12]', '[fade start=4 length=12]Fading into the fog[/fade]'},
    {'[rainbow freq=1 sat=0.8 val=0.9]', '[rainbow freq=1 sat=0.8 val=0.9]Northern lights[/rainbow]'},
    {'[pulse freq=2 color=#FFFF8A00 ease=-2]', '[pulse freq=2 color=#FFFF8A00 ease=-2]The beacon burns[/pulse]'},
    {'[wave][rainbow] nested', '[wave amp=12][rainbow]Treasure![/rainbow][/wave]'},
}

local kDialogue = {
    '[b]Keeper:[/b] You made it through the storm.[pause=0.6] Few ships do.',
    '[b]Keeper:[/b] The beacon went dark three nights ago.[pause=0.5] [shake]Something[/shake] is out there.',
    '[b]Keeper:[/b] Take this key [img=images/star.png height=34] and [speed=0.3]be careful...[/speed][pause=0.4] [rainbow]good luck![/rainbow]',
}

function Typewriter:init(entry)
    Typewriter.super.init(self, entry)
    self.line = 1
    self.speed = 30
    local family = fonts.family('crimson')
    self.effects = {}
    for index, effect in ipairs(kEffects) do
        self.effects[index] = {label = effect[1], text = graphics2d.newRichText(effect[2], {family = family, size = 44})}
    end
    self:say()
end

-- Starts the current line of the dialogue with the reveal speed of the slider.
function Typewriter:say()
    self.dialogue = graphics2d.newRichText(kDialogue[self.line], {family = fonts.family('crimson'), size = 40, maxWidth = 1300, revealSpeed = self.speed})
end

function Typewriter:controls()
    return {
        ui.button{id = 'next', text = 'Next line', variant = 'primary', align = 'stretch', onClick = function()
            self.line = self.line % #kDialogue + 1
            self:say()
        end},
        ui.row{gap = 12,
            ui.button{text = 'Skip', grow = 1, onClick = function() self.dialogue.visibleCharacters = -1 end},
            ui.button{text = 'Replay', grow = 1, onClick = function() self:say() end},
        },
        ui.label{text = 'Characters per second'},
        ui.slider{id = 'speed', min = 5, max = 80, step = 5, value = self.speed, showValue = true, decimals = 0, onChange = function(event)
            self.speed = event.value
            self:say()
        end},
        ui.progress{id = 'revealed', text = 'Revealed'},
    }
end

function Typewriter:update(dt)
    for _, effect in ipairs(self.effects) do
        effect.text:update(dt)
    end
    local dialogue = self.dialogue
    dialogue:update(dt)
    self.document:set('revealed', {value = dialogue.visibleRatio})
    self:setStatus(string.format('Line %d, %d of %d characters, %s', self.line, dialogue.revealing and dialogue.visibleCharacters or dialogue.characterCount, dialogue.characterCount, dialogue.revealing and 'revealing' or 'done'))
end

function Typewriter:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    graphics2d.beginScreen()
    local y = stage.y
    for _, effect in ipairs(self.effects) do
        sample.caption(effect.label, stage.x, y + 14)
        effect.text:draw(stage.x + 520, y)
        y = y + 64
    end
    local box = {stage.x, stage:bottom() - 170, stage.width, 170}
    graphics2d.drawRect(box, '#E0101828')
    graphics2d.drawRectOutline(box, 3, '#FF8FB0FF')
    self.dialogue:draw(box[1] + 30, box[2] + 24)
end

return Typewriter
