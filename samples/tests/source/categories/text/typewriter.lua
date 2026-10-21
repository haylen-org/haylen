-- Effects and typewriter: the built-in effects animate their glyphs from the time alone, with the attributes of their tags, and a typewriter reveals a dialogue with `[pause]` and `[speed]` tags in a dialogue box drawn from the nine-slices of the fantasy theme.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local Typewriter = haylen.class('Typewriter', TextTest)

Typewriter.code = [[
local wave = graphics2d.newRichText('[wave amp=8 freq=2]A gentle swell[/wave]', {family = family, size = 44})
local spoken = graphics2d.newRichText(line, {family = family, size = 36, maxWidth = 1100, revealSpeed = 30})
spoken:update(dt)  -- Reveals 30 characters per second.
spoken.visibleCharacters = -1  -- Skips to the end.
graphics2d.drawNineSlice(panel, box, '#FFFFFFFF', {borderScale = 0.5})]]

Typewriter.effects = {
    {'[wave]', '[wave]The sea rolls[/wave]'},
    {'[wave amp=8 freq=2]', '[wave amp=8 freq=2]A gentle swell[/wave]'},
    {'[shake rate=20 level=5]', '[shake rate=20 level=5]Thunder![/shake]'},
    {'[tornado radius=10 freq=1]', '[tornado radius=10 freq=1]A whirlpool[/tornado]'},
    {'[fade start=2 length=24]', '[fade start=2 length=24]Fading into the fog[/fade]'},
    {'[rainbow freq=1 sat=0.8 val=0.9]', '[rainbow freq=1 sat=0.8 val=0.9]Northern lights[/rainbow]'},
    {'[pulse freq=2 color=#FFFF8A00 ease=-2]', '[pulse freq=2 color=#FFFF8A00 ease=-2]The beacon burns[/pulse]'},
    {'[wave amp=12][rainbow]', '[wave amp=12][rainbow]Treasure![/rainbow][/wave]'},
}
Typewriter.dialogue = {
    'You made it through the storm.[pause=0.6] Few ships do.',
    'The beacon went dark three nights ago.[pause=0.5] [shake]Something[/shake] is out there.',
    'Take this key [img=text/star.png height=34] and [speed=0.3]be careful...[/speed][pause=0.4] [rainbow]good luck![/rainbow]',
}
Typewriter.speaker = 'Keeper'
Typewriter.boxHeight = 200
Typewriter.boxPadding = {44, 40}

function Typewriter:init(entry)
    Typewriter.super.init(self, entry)
    self.line = 1
    self.speed = 30
    self.family = fonts.family('crimson')
    self.mono = fonts.load('mono')
    self.samples = {}
    for index, effect in ipairs(Typewriter.effects) do
        self.samples[index] = {label = effect[1], text = graphics2d.newRichText(effect[2], {family = self.family, size = 44, color = Test.ink})}
    end
    local atlas = assets.texture('interface/themes/fantasy/atlas.png', {filter = 'linear'})
    self.panel = graphics2d.newNineSlice(atlas, {source = {0, 0, 320, 320}, borders = {60, 60, 60, 60}})
    self.plate = graphics2d.newNineSlice(atlas, {source = {0, 516, 480, 112}, borders = {40, 48, 40, 48}})
    self.speakerText = graphics2d.newRichText('[b]' .. Typewriter.speaker .. '[/b]', {family = self.family, size = 34, color = '#FFFFFFFF'})
    self:say()
end

-- Starts the current line of the dialogue with the reveal speed of the slider.
function Typewriter:say()
    self.spoken = graphics2d.newRichText(Typewriter.dialogue[self.line], {family = self.family, size = 36, maxWidth = self.textWidth or 1100, revealSpeed = self.speed, color = Test.ink})
end

function Typewriter:enter()
    self:frame{
        code = Typewriter.code,
        hint = 'Next line reveals the next line of the dialogue, Skip shows the rest at once and Replay starts the line over. The speed slider sets the characters per second, and "[speed]" tags change it inside a line.',
        controls = {
            ui.button{id = 'next', text = 'Next line', variant = 'primary', align = 'stretch', onClick = function()
                self.line = self.line % #Typewriter.dialogue + 1
                self:say()
            end},
            ui.row{wrap = true, gap = 12,
                ui.button{id = 'skip', text = 'Skip', grow = 1, onClick = function() self.spoken.visibleCharacters = -1 end},
                ui.button{id = 'replay', text = 'Replay', grow = 1, onClick = function() self:say() end},
            },
            ui.formField{label = 'Characters per second', ui.slider{id = 'speed', min = 5, max = 80, step = 5, value = self.speed, showValue = true, decimals = 0, onChange = function(event)
                self.speed = event.value
                self:say()
            end}},
            ui.formField{label = 'Revealed so far', ui.progress{id = 'revealed'}},
        },
        focus = 'next',
    }
end

-- The dialogue wraps at the width of its box, which follows the page.
function Typewriter:resize(area)
    Typewriter.super.resize(self, area)
    self.textWidth = self.layout.width - Typewriter.boxPadding[1] * 2
    self.spoken.maxWidth = self.textWidth
end

function Typewriter:update(dt)
    Typewriter.super.update(self, dt)
    for _, sample in ipairs(self.samples) do
        sample.text:update(dt)
    end
    local spoken = self.spoken
    spoken:update(dt)
    self:set('revealed', {value = spoken.visibleRatio})
    local shown = spoken.revealing and spoken.visibleCharacters or spoken.characterCount
    self:status(string.format('Line %d of %d shows %d of its %d characters%s', self.line, #Typewriter.dialogue, shown, spoken.characterCount, spoken.revealing and ' and keeps revealing.' or ', all of them.'))
end

-- Draws the effects in two columns, each tag in the mono font above the text it animates.
function Typewriter:effectRows(height)
    local layout = self.layout
    local rows = #self.samples / 2
    local pitch = height / rows
    for index, sample in ipairs(self.samples) do
        local column, row = (index - 1) // rows, (index - 1) % rows
        local x, y = column * layout.width / 2, row * pitch
        graphics2d.drawText(self.mono, sample.label, x, y, {size = 20, color = Test.accent})
        sample.text:draw(x, y + 30)
    end
end

-- The dialogue box: the panel of the theme, the plate with the name of the speaker over its top edge, and a marker that bobs once the line is shown.
function Typewriter:dialogueBox(box)
    local padding = Typewriter.boxPadding
    graphics2d.drawNineSlice(self.panel, box, '#FFFFFFFF', {borderScale = 0.5})
    self.spoken:draw(box[1] + padding[1], box[2] + padding[2])

    local nameWidth, nameHeight = self.speakerText:size()
    local plate = {box[1] + 28, box[2] - 30, nameWidth + 64, 58}
    graphics2d.drawNineSlice(self.plate, plate, '#FFFFFFFF', {borderScale = 0.55, layer = 1})
    self.speakerText:draw(plate[1] + 32, plate[2] + (plate[4] - nameHeight) / 2 - 2, {layer = 2})

    if not self.spoken.revealing then
        local bob = math.abs(math.sin(haylen.elapsed() * 4)) * 6
        local x, y = box[1] + box[3] - 44, box[2] + box[4] - 40 + bob
        graphics2d.drawPolygon({{x - 14, y - 10}, {x + 14, y - 10}, {x, y + 8}}, '#FFFFC93C', {layer = 1})
    end
end

function Typewriter:draw(area)
    local layout = self.layout
    local boxTop = layout.height - Typewriter.boxHeight
    self:effectRows(boxTop - 50)
    self:dialogueBox({0, boxTop, layout.width, Typewriter.boxHeight})
end

return Typewriter
