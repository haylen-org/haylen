-- Sprite basics: one hero whose pivot, rotation, scale, flips, tint and flash the panel changes, above a row of heroes on the grass that each show one property on its own, with the pivot of every sprite marked.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Landscape = require('categories.sprites.landscape')
local Plate = require('categories.sprites.plate')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Basics = haylen.class('Basics', SpriteTest)

Basics.pivots = {{id = 'center', text = 'Center', x = 0.5, y = 0.5, words = 'at the center'}, {id = 'feet', text = 'Feet', x = 0.5, y = 1, words = 'at the feet'}, {id = 'corner', text = 'Corner', x = 0, y = 0, words = 'at the top-left corner'}}
Basics.tints = {{id = 'none', text = 'Off', color = '#FFFFFFFF', words = 'no tint'}, {id = 'red', text = 'Red', color = '#FFFF7070', words = 'tinted red'}, {id = 'gold', text = 'Gold', color = '#FFFFD166', words = 'tinted gold'}, {id = 'ghost', text = 'Ghost', color = '#70FFFFFF', words = 'see-through'}}
-- The hero is painted at twice the size it draws at.
Basics.size = 96
Basics.scale = 2.5
Basics.groundHeight = 76
Basics.variants = {
    {label = 'Corner pivot', properties = {pivotX = 0, pivotY = 0, rotation = -0.4}},
    {label = 'Stretched', properties = {scaleX = 1.5, scaleY = 0.75}},
    {label = 'Mirrored', properties = {flipHorizontal = true}},
    {label = 'Upside down', properties = {flipVertical = true}},
    {label = 'Tinted blue', properties = {color = '#FF70C0FF'}},
    {label = 'Flashed white', properties = {flash = '#B0FFFFFF'}},
}
Basics.markInk = '#FFFFE45C'
Basics.markOutline = '#FF1E1B33'

Basics.code = [[
local hero = graphics2d.newSprite(texture, {pivotX = 0.5, pivotY = 1, scaleX = 2.5, scaleY = 2.5})
hero.rotation = hero.rotation + dt
hero.flipHorizontal = true
hero.color = '#FFFF7070'
tween.fromTo(hero, 0.4, {flash = '#FFFFFFFF'}, {flash = '#00FFFFFF'})
hero:draw()]]

-- Returns the items of a segmented control, which take only an id and a text.
function Basics.items(choices)
    local list = {}
    for index, choice in ipairs(choices) do
        list[index] = {id = choice.id, text = choice.text}
    end
    return list
end

function Basics.find(choices, id)
    for _, choice in ipairs(choices) do
        if choice.id == id then
            return choice
        end
    end
end

function Basics:enter()
    local texture = SpriteTest.texture('images/hero.png')
    local size = Basics.size
    self.landscape = Landscape()
    self.time = 0
    self.hero = graphics2d.newSprite(texture, {width = size, height = size, scaleX = Basics.scale, scaleY = Basics.scale, layer = 2})
    self.pivot, self.tint = Basics.pivots[1], Basics.tints[1]
    self.samples = {}
    for index, variant in ipairs(Basics.variants) do
        self.samples[index] = graphics2d.newSprite(texture, variant.properties)
        self.samples[index].width, self.samples[index].height, self.samples[index].layer = size, size, 2
    end
    self.spin = true
    self.sliderTime = 0
    self:frame{
        code = Basics.code,
        hint = 'The yellow cross marks the pivot: the point that sits at the position of a sprite and that rotation turns around.',
        controls = {
            ui.formField{label = 'Pivot', ui.segmentedControl{id = 'pivot', items = Basics.items(Basics.pivots), selected = 'center', onChange = function(event)
                self.pivot = Basics.find(Basics.pivots, event.value)
                self.hero.pivotX, self.hero.pivotY = self.pivot.x, self.pivot.y
            end}},
            ui.toggle{id = 'spin', text = 'Spin', checked = true, onChange = function(event) self.spin = event.checked end},
            ui.formField{label = 'Rotation in degrees', ui.slider{id = 'rotation', min = 0, max = 360, value = 0, step = 15, decimals = 0, showValue = true, onChange = function(event)
                self.hero.rotation = math.rad(event.value)
                self.spin = false
                self.gui:set('spin', {checked = false})
            end}},
            ui.formField{label = 'Scale', ui.slider{id = 'scale', min = 0.5, max = 3, value = Basics.scale, step = 0.1, decimals = 1, showValue = true, onChange = function(event)
                self.hero.scaleX, self.hero.scaleY = event.value, event.value
            end}},
            ui.checkbox{id = 'flipHorizontal', text = 'Mirror', onChange = function(event) self.hero.flipHorizontal = event.checked end},
            ui.checkbox{id = 'flipVertical', text = 'Turn upside down', onChange = function(event) self.hero.flipVertical = event.checked end},
            ui.formField{label = 'Tint', ui.segmentedControl{id = 'tint', items = Basics.items(Basics.tints), selected = 'none', onChange = function(event)
                self.tint = Basics.find(Basics.tints, event.value)
                self.hero.color = self.tint.color
            end}},
            ui.button{id = 'flash', text = 'Flash white', variant = 'primary', align = 'stretch', onClick = function()
                tween.fromTo(self.hero, 0.4, {flash = '#FFFFFFFF'}, {flash = '#00FFFFFF'}, {owner = self})
            end},
        },
        focus = 'pivot',
    }
end

-- The row stands on the grass, every sprite placed so the bottom of its box rests on the ground whatever its pivot and scale, under the hero in the sky.
function Basics:resize(area)
    self.ground = area.height - Basics.groundHeight
    local size = Basics.size
    -- The hero hangs in the middle of the sky, and no higher than a little above the row on a tall stage.
    self.hero.x, self.hero.y = area.width * 0.5, math.max((self.ground - size - 40) * 0.5, self.ground - size - 420)
    local step = area.width / #self.samples
    for index, sprite in ipairs(self.samples) do
        local width, height = size * sprite.scaleX, size * sprite.scaleY
        sprite.x = step * (index - 0.5) - (0.5 - sprite.pivotX) * width
        sprite.y = self.ground - 6 - (1 - sprite.pivotY) * height
    end
end

function Basics:update(dt)
    Basics.super.update(self, dt)
    self.time = self.time + dt
    local hero = self.hero
    if self.spin then
        hero.rotation = (hero.rotation + dt) % m.tau
    end
    -- The slider follows the hero while it spins, a few times per second like the status.
    self.sliderTime = self.sliderTime + dt
    if self.spin and self.sliderTime >= Test.statusInterval then
        self.sliderTime = 0
        self.gui:set('rotation', {value = math.deg(hero.rotation)})
    end
    local flips = hero.flipHorizontal and (hero.flipVertical and 'mirrored and upside down' or 'mirrored') or (hero.flipVertical and 'upside down' or 'not flipped')
    local degrees = math.floor(math.deg(hero.rotation) + 0.5) % 360
    self:status(string.format('Pivot %s, turned %d degrees, %.1f times as large, %s, %s.', self.pivot.words, degrees, hero.scaleX, flips, self.tint.words))
end

-- Marks a pivot with a cross outlined in dark navy, which reads over the sky and over any sprite.
function Basics.mark(x, y, reach)
    local outline, ink = Basics.markOutline, Basics.markInk
    graphics2d.drawShape({x - reach - 3, y - 4.5, reach * 2 + 6, 9}, {radius = 4.5, color = outline, layer = 10})
    graphics2d.drawShape({x - 4.5, y - reach - 3, 9, reach * 2 + 6}, {radius = 4.5, color = outline, layer = 10})
    graphics2d.drawShape({x - reach, y - 1.5, reach * 2, 3}, {radius = 1.5, color = ink, layer = 11})
    graphics2d.drawShape({x - 1.5, y - reach, 3, reach * 2}, {radius = 1.5, color = ink, layer = 11})
    graphics2d.drawCircle(x, y, 6, outline, {layer = 11})
    graphics2d.drawCircle(x, y, 3.5, ink, {layer = 12})
end

function Basics:draw(area)
    self.landscape:draw(area, self.ground, 0.5, self.time)
    local hero = self.hero
    hero:draw()
    Basics.mark(hero.x, hero.y, 22)

    local step = area.width / #self.samples
    for index, sprite in ipairs(self.samples) do
        local x = step * (index - 0.5)
        graphics2d.drawEllipse(x, self.ground - 4, 40, 9, '#40000000', {layer = 1})
        sprite:draw()
        Basics.mark(sprite.x, sprite.y, 12)
        Plate.draw(Basics.variants[index].label, x, self.ground + Basics.groundHeight * 0.5)
    end
end

return Basics
