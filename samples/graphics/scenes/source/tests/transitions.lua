-- Transition gallery: every built-in effect of haylen.scene pushes a card with the chosen direction, easing, duration and color, and the card pops itself back with the opposite direction.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('card')
local sample = require('sample')

local Transitions = haylen.class('Transitions', sample.Test)

Transitions.hints = 'Pick an effect to push a card with it. The card leaves by itself after a moment, or with Escape or the B button.'

Transitions.effects = {'fade', 'crossFade', 'moveIn', 'slideIn', 'push', 'shrinkGrow', 'flipX', 'flipY', 'zoomFlip', 'rotoZoom', 'jumpZoom', 'splitColumns', 'splitRows', 'turnOffTiles', 'fadeTiles', 'pageTurn', 'radialClockwise', 'radialCounterclockwise', 'wipe', 'inOut', 'outIn', 'iris', 'dissolve', 'pixelate'}
Transitions.directions = {'left', 'right', 'up', 'down', 'upLeft', 'upRight', 'downLeft', 'downRight'}
Transitions.opposite = {left = 'right', right = 'left', up = 'down', down = 'up', upLeft = 'downRight', upRight = 'downLeft', downLeft = 'upRight', downRight = 'upLeft'}
Transitions.eases = {'linear', 'sineInOut', 'quadOut', 'cubicInOut', 'expoInOut', 'backOut', 'elasticOut', 'bounceOut'}
Transitions.colors = {'#FF2E5E8A', '#FF8A3E5E', '#FF3E8A5E', '#FF8A6E2E', '#FF5E3E8A'}

-- Turns a list of names into the items of a picker.
function Transitions.items(names)
    local items = {}
    for index, name in ipairs(names) do
        items[index] = {id = name, text = name}
    end
    return items
end

function Transitions:init(entry)
    Transitions.super.init(self, entry)
    self.options = {direction = 'left', ease = 'cubicInOut', duration = 0.8, color = '#FF000000'}
    self.played = 0
end

function Transitions:controls()
    local options = self.options
    return {
        ui.formField{label = 'Direction', ui.combo{items = Transitions.items(Transitions.directions), selected = options.direction, onChange = function(event)
            options.direction = event.value
        end}},
        ui.formField{label = 'Easing', ui.combo{items = Transitions.items(Transitions.eases), selected = options.ease, onChange = function(event)
            options.ease = event.value
        end}},
        ui.formField{label = 'Duration in seconds', ui.slider{min = 0.2, max = 2.5, value = options.duration, showValue = true, onChange = function(event)
            options.duration = event.value
        end}},
        ui.formField{label = 'Color of fades and irises', ui.colorField{value = options.color, alpha = false, onChange = function(event)
            options.color = event.value
        end}},
    }
end

function Transitions:enter()
    Transitions.super.enter(self)
    local buttons = {}
    for index, effect in ipairs(Transitions.effects) do
        buttons[index] = ui.button{id = effect, text = effect, onClick = function()
            self:play(effect)
        end}
    end
    self.gallery = ui.mount(ui.column{padding = {250, 540, 110, 24}, onCancel = function() self:cancel() end, ui.grid{columns = 4, gap = 12, children = buttons}}, {owner = self})
end

-- Hides the interface while a card covers the gallery, which the scene receives at the switch of the change, so the card never shows it.
function Transitions:pause()
    self.gallery.visible = false
    self.header.visible = false
end

function Transitions:resume()
    self.gallery.visible = true
    self.header.visible = true
end

function Transitions:play(effect)
    if scene.transitioning() then
        return
    end
    local options = self.options
    local arrive = {effect = effect, direction = options.direction, ease = options.ease, duration = options.duration, color = options.color}
    local leave = {effect = effect, direction = Transitions.opposite[options.direction], ease = options.ease, duration = options.duration, color = options.color}
    self.played = self.played + 1
    local color = Transitions.colors[self.played % #Transitions.colors + 1]
    scene.push(Card({title = effect, caption = options.direction .. ', ' .. options.ease .. string.format(', %.1f s', options.duration), color = color, stay = 1.2, leave = leave}), arrive)
    self:setStatus(string.format('pushed with %s, %d played', effect, self.played))
end

function Transitions:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF1C2230')
    for index = 0, 14 do
        graphics2d.drawRect({area.x + index * 140, area.y, 70, area.height}, '#FF20283A')
    end
end

return Transitions
