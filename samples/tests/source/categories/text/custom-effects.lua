-- Custom effects: `graphics2d.registerTextEffect` adds a tag whose Lua function moves, colors and hides each glyph every frame from its index, its position and the time, and `graphics2d.registerTextIcon` adds images that `[icon]` shows inline.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local TextTest = require('categories.text.text-test')
local fonts = require('categories.text.fonts')

local CustomEffects = haylen.class('CustomEffects', TextTest)

CustomEffects.lines = {
    {'[ghost speed=2]', '[ghost speed=2]The ghost of the old keeper[/ghost]'},
    {'[bounce height=20]', '[bounce height=20]Jump over the waves![/bounce]'},
    {'[glitch rate=8]', '[glitch rate=8]SIGNAL LOST, SIGNAL FOUND[/glitch]'},
    {'[heartbeat bpm=90]', '[heartbeat bpm=90]Low health[/heartbeat] [icon=heart]'},
    {'[drop cycle=4]', '[drop cycle=4]Treasure found[/drop] [icon=coin] [icon=coin]'},
    {'[blink rate=2]', 'Press any key[blink rate=2]_[/blink]'},
    {'[wave amp=6][ghost]', '[wave amp=6][ghost]A haunted tide[/ghost][/wave]'},
}

-- A ghost fades in and out along the word and floats up as it shows.
function CustomEffects.ghost(glyph, attributes)
    local alpha = 0.5 + 0.5 * math.sin(glyph.time * (attributes.speed or 2) + glyph.index * 0.6)
    glyph.color = string.format('#%02XE0F0FF', math.floor(alpha * 255))
    glyph.offsetY = glyph.offsetY - alpha * 8
end

-- Letters hop one after another, as high as the `height` attribute.
function CustomEffects.bounce(glyph, attributes)
    glyph.offsetY = glyph.offsetY - math.abs(math.sin(glyph.time * 5 + glyph.index * 0.45)) * (attributes.height or 16)
end

-- A glitch jumps sideways and flashes cyan on some glyphs, a few times a second.
function CustomEffects.glitch(glyph, attributes)
    local tick = math.floor(glyph.time * (attributes.rate or 8))
    local noise = (tick * 7919 + glyph.index * 104729) % 97
    if noise < 18 then
        glyph.offsetX = glyph.offsetX + (noise % 3 - 1) * 6
        glyph.color = '#FF40FFFF'
    end
end

-- A heartbeat reddens the word on each beat and lifts it.
function CustomEffects.heartbeat(glyph, attributes)
    local phase = (glyph.time * (attributes.bpm or 72) / 60) % 1
    local beat = math.max(0, 1 - phase * 6)
    glyph.color = string.format('#FFFF%02X%02X', math.floor(255 - beat * 200), math.floor(255 - beat * 200))
    glyph.offsetY = glyph.offsetY - beat * 6
end

-- Letters drop into place one by one and start over every few seconds.
function CustomEffects.drop(glyph, attributes)
    local cycle = attributes.cycle or 4
    local elapsed = glyph.time % cycle - glyph.index * 0.08
    glyph.visible = elapsed > 0
    glyph.offsetY = glyph.offsetY - math.max(0, 1 - elapsed * 4) * 60
end

-- A blink like a text cursor hides the glyphs half of the time.
function CustomEffects.blink(glyph, attributes)
    glyph.visible = math.floor(glyph.time * (attributes.rate or 2)) % 2 == 0
end

function CustomEffects:init(entry)
    CustomEffects.super.init(self, entry)
    for _, name in ipairs({'ghost', 'bounce', 'glitch', 'heartbeat', 'drop', 'blink'}) do
        graphics2d.registerTextEffect(name, CustomEffects[name])
    end
    graphics2d.registerTextIcon('coin', assets.texture('text/coin.png', {filter = 'linear'}))
    graphics2d.registerTextIcon('heart', assets.texture('text/heart.png', {filter = 'linear'}))

    local family = fonts.family('lilita')
    self.samples = {}
    for index, line in ipairs(CustomEffects.lines) do
        self.samples[index] = {label = line[1], text = graphics2d.newRichText(line[2], {family = family, size = 52})}
    end
    self.names = '"' .. table.concat(graphics2d.textEffectNames(), '", "') .. '"'
end

function CustomEffects:enter()
    self:frame{hint = 'Each effect is a few lines of Lua in "source/categories/text/custom-effects.lua". An effect sees the glyph index inside its tag, the character in the whole text, the pen position and the time, and the same time always gives the same picture.'}
end

function CustomEffects:update(dt)
    CustomEffects.super.update(self, dt)
    for _, sample in ipairs(self.samples) do
        sample.text:update(dt)
    end
    self:status('Registered effects: ' .. self.names)
end

function CustomEffects:draw(area)
    local y = 10
    for _, sample in ipairs(self.samples) do
        Test.caption(sample.label, 0, y + 22, {size = 22})
        sample.text:draw(420, y)
        y = y + 92
    end
end

return CustomEffects
