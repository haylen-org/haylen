-- Sprite basics: one hero whose pivot, rotation, scale, flips, tint and flash the panel changes, next to a row of sprites that each show one property on its own.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local sample = require('sample')

local Basics = haylen.class('Basics', sample.Test)

local kPivots = {{id = 'center', text = 'Center', x = 0.5, y = 0.5}, {id = 'feet', text = 'Feet', x = 0.5, y = 1}, {id = 'corner', text = 'Corner', x = 0, y = 0}}
local kTints = {{id = 'none', text = 'None', color = '#FFFFFFFF'}, {id = 'red', text = 'Red', color = '#FFFF7070'}, {id = 'gold', text = 'Gold', color = '#FFFFD166'}, {id = 'ghost', text = 'Ghost', color = '#70FFFFFF'}}
local kVariants = {
    {label = 'pivot 0, 0 turned', properties = {pivotX = 0, pivotY = 0, rotation = 0.4}},
    {label = 'scale 1.5, 0.75', properties = {scaleX = 1.5, scaleY = 0.75}},
    {label = 'flipHorizontal', properties = {flipHorizontal = true}},
    {label = 'flipVertical', properties = {flipVertical = true}},
    {label = 'color', properties = {color = '#FF70C0FF'}},
    {label = 'flash', properties = {flash = '#B0FFFFFF'}},
}
local kCode = [[
local hero = graphics2d.newSprite(assets.texture('images/hero.png'), {x = 300, y = 300, pivotX = 0.5, pivotY = 1})
hero.rotation, hero.scaleX, hero.flipHorizontal, hero.color = 0.3, 2, true, '#FFFF7070'
tween.fromTo(hero, 0.4, {flash = '#FFFFFFFF'}, {flash = '#00FFFFFF'})  -- A hit flash.
hero:draw()]]

-- Returns the items of a list of choices, which take only an id and a text.
local function items(choices)
    local list = {}
    for index, choice in ipairs(choices) do
        list[index] = {id = choice.id, text = choice.text}
    end
    return list
end

local function find(choices, id)
    for _, choice in ipairs(choices) do
        if choice.id == id then
            return choice
        end
    end
end

function Basics:enter()
    local texture = sample.texture('images/hero.png')
    self.hero = graphics2d.newSprite(texture, {scaleX = 2, scaleY = 2})
    self.variants = {}
    for index, variant in ipairs(kVariants) do
        self.variants[index] = graphics2d.newSprite(texture, variant.properties)
    end
    self.spin = true
    self:frame({
        hint = 'The cross marks the pivot: the point that sits at the position and that rotation turns around.',
        code = kCode,
        controls = {
            ui.formField{label = 'Pivot', ui.segmentedControl{id = 'pivot', items = items(kPivots), selected = 'center', onChange = function(event)
                local pivot = find(kPivots, event.value)
                self.hero.pivotX, self.hero.pivotY = pivot.x, pivot.y
            end}},
            ui.toggle{id = 'spin', text = 'Spin', checked = true, onChange = function(event) self.spin = event.checked end},
            ui.formField{label = 'Scale', ui.slider{id = 'scale', min = 0.5, max = 3, value = 2, step = 0.1, showValue = true, onChange = function(event)
                self.hero.scaleX, self.hero.scaleY = event.value, event.value
            end}},
            ui.row{gap = 12,
                ui.checkbox{id = 'flipHorizontal', text = 'flipHorizontal', onChange = function(event) self.hero.flipHorizontal = event.checked end},
                ui.checkbox{id = 'flipVertical', text = 'flipVertical', onChange = function(event) self.hero.flipVertical = event.checked end},
            },
            ui.formField{label = 'Tint', ui.segmentedControl{id = 'tint', items = items(kTints), selected = 'none', onChange = function(event)
                self.hero.color = find(kTints, event.value).color
            end}},
            ui.button{id = 'flash', text = 'Flash', variant = 'primary', onClick = function()
                tween.fromTo(self.hero, 0.4, {flash = '#FFFFFFFF'}, {flash = '#00FFFFFF'}, {owner = self})
            end},
        },
        focus = 'pivot',
    })
end

function Basics:resize(area)
    self.hero.x, self.hero.y = area.width * 0.5, area.height * 0.38
    local step = area.width / #self.variants
    for index, sprite in ipairs(self.variants) do
        -- A top-left pivot puts the corner on the spot, so that sprite moves back by half its size to share the row.
        local shift = 48 - sprite.pivotX * 96
        sprite.x, sprite.y = step * (index - 0.5) - shift, area.height - 130 - shift
    end
end

function Basics:update(dt)
    Basics.super.update(self, dt)
    if self.spin then
        self.hero.rotation = self.hero.rotation + dt
    end
    local hero = self.hero
    self:status(string.format('pivot %.1f, %.1f   rotation %.2f   scale %.1f   flip %s %s   color %s', hero.pivotX, hero.pivotY, hero.rotation % (math.pi * 2), hero.scaleX, hero.flipHorizontal, hero.flipVertical, hero.color:toHex()))
end

function Basics:draw(area)
    local hero = self.hero
    hero:draw()
    graphics2d.drawLine(hero.x - 18, hero.y, hero.x + 18, hero.y, 3, sample.warm, {layer = 1})
    graphics2d.drawLine(hero.x, hero.y - 18, hero.x, hero.y + 18, 3, sample.warm, {layer = 1})

    graphics2d.drawLine(0, area.height - 230, area.width, area.height - 230, 1, sample.line)
    local step = area.width / #self.variants
    for index, sprite in ipairs(self.variants) do
        sprite:draw()
        graphics2d.drawCircle(sprite.x, sprite.y, 4, sample.warm, {layer = 1})
        graphics2d.drawText(nil, kVariants[index].label, step * (index - 0.5), area.height - 30, {size = 22, color = sample.muted, anchor = {0.5, 0.5}})
    end
end

return Basics
