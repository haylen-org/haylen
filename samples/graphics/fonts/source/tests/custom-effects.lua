-- Custom effects: graphics2d.registerTextEffect adds a tag whose Lua function moves, colors and hides each glyph every frame from its index, its position and the time, and graphics2d.registerTextIcon adds images that [icon] shows inline.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local fonts = require('fonts')
local sample = require('sample')

local CustomEffects = haylen.class('CustomEffects', sample.Test)

CustomEffects.hints = 'Each effect below is a few lines of Lua in source/tests/custom-effects.lua. An effect sees the glyph index inside its tag, the character in the whole text, the pen position and the time, and the same time always gives the same picture.'

-- A ghost fades in and out along the word and floats up as it shows.
graphics2d.registerTextEffect('ghost', function(glyph, attributes)
    local alpha = 0.5 + 0.5 * math.sin(glyph.time * (attributes.speed or 2) + glyph.index * 0.6)
    glyph.color = string.format('#%02XE0F0FF', math.floor(alpha * 255))
    glyph.offsetY = glyph.offsetY - alpha * 8
end)

-- Letters hop one after another, as high as the height attribute.
graphics2d.registerTextEffect('bounce', function(glyph, attributes)
    glyph.offsetY = glyph.offsetY - math.abs(math.sin(glyph.time * 5 + glyph.index * 0.45)) * (attributes.height or 16)
end)

-- A glitch jumps sideways and flashes cyan on some glyphs, a few times a second.
graphics2d.registerTextEffect('glitch', function(glyph, attributes)
    local tick = math.floor(glyph.time * (attributes.rate or 8))
    local noise = (tick * 7919 + glyph.index * 104729) % 97
    if noise < 18 then
        glyph.offsetX = glyph.offsetX + (noise % 3 - 1) * 6
        glyph.color = '#FF40FFFF'
    end
end)

-- A heartbeat reddens the word twice a second and lifts it on the beat.
graphics2d.registerTextEffect('heartbeat', function(glyph, attributes)
    local phase = (glyph.time * (attributes.bpm or 72) / 60) % 1
    local beat = math.max(0, 1 - phase * 6)
    glyph.color = string.format('#FFFF%02X%02X', math.floor(255 - beat * 200), math.floor(255 - beat * 200))
    glyph.offsetY = glyph.offsetY - beat * 6
end)

-- Letters drop into place one by one and start over every few seconds.
graphics2d.registerTextEffect('drop', function(glyph, attributes)
    local cycle = attributes.cycle or 4
    local elapsed = glyph.time % cycle - glyph.index * 0.08
    glyph.visible = elapsed > 0
    glyph.offsetY = glyph.offsetY - math.max(0, 1 - elapsed * 4) * 60
end)

-- A cursor-like blink that hides the glyphs half of the time.
graphics2d.registerTextEffect('blink', function(glyph, attributes)
    glyph.visible = math.floor(glyph.time * (attributes.rate or 2)) % 2 == 0
end)

graphics2d.registerTextIcon('coin', assets.texture('images/coin.png', {filter = 'linear'}))
graphics2d.registerTextIcon('heart', assets.texture('images/heart.png', {filter = 'linear'}))

local kLines = {
    {'ghost speed=2', '[ghost speed=2]The ghost of the old keeper[/ghost]'},
    {'bounce height=20', '[bounce height=20]Jump over the waves![/bounce]'},
    {'glitch rate=8', '[glitch rate=8]SIGNAL LOST, SIGNAL FOUND[/glitch]'},
    {'heartbeat bpm=90', '[heartbeat bpm=90]Low health[/heartbeat] [icon=heart]'},
    {'drop cycle=4', '[drop cycle=4]Treasure found[/drop] [icon=coin] [icon=coin]'},
    {'blink rate=2', 'Press any key[blink rate=2]_[/blink]'},
    {'nested with built-ins', '[wave amp=6][ghost]A haunted tide[/ghost][/wave]'},
}

function CustomEffects:init(entry)
    CustomEffects.super.init(self, entry)
    self.lines = {}
    for index, line in ipairs(kLines) do
        self.lines[index] = {label = line[1], text = graphics2d.newRichText(line[2], {family = fonts.family('lilita'), size = 52})}
    end
    self.effects = table.concat(graphics2d.textEffects(), ', ')
end

function CustomEffects:update(dt)
    for _, line in ipairs(self.lines) do
        line.text:update(dt)
    end
    self:setStatus('registered effects: ' .. self.effects)
end

function CustomEffects:render()
    local stage = self:stage()
    if stage == nil then
        return
    end
    graphics2d.beginScreen()
    local y = stage.y + 10
    for _, line in ipairs(self.lines) do
        sample.caption('[' .. line.label .. ']', stage.x, y + 22)
        line.text:draw(stage.x + 420, y)
        y = y + 92
    end
end

return CustomEffects
