-- Transition gallery: every built-in effect of `haylen.scene` pushes a card with the chosen direction, easing, duration and color, and the card pops itself back with the opposite direction.
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')

local Transitions = haylen.class('Transitions', SceneTest)

Transitions.effects = {'fade', 'crossFade', 'moveIn', 'slideIn', 'push', 'shrinkGrow', 'flipX', 'flipY', 'zoomFlip', 'rotoZoom', 'jumpZoom', 'splitColumns', 'splitRows', 'turnOffTiles', 'fadeTiles', 'pageTurn', 'radialClockwise', 'radialCounterclockwise', 'wipe', 'inOut', 'outIn', 'iris', 'dissolve', 'pixelate'}
Transitions.directions = {'left', 'right', 'up', 'down', 'upLeft', 'upRight', 'downLeft', 'downRight'}
Transitions.opposite = {left = 'right', right = 'left', up = 'down', down = 'up', upLeft = 'downRight', upRight = 'downLeft', downLeft = 'upRight', downRight = 'upLeft'}
Transitions.eases = {'linear', 'sineInOut', 'quadOut', 'cubicInOut', 'expoInOut', 'backOut', 'elasticOut', 'bounceOut'}
Transitions.colors = {'#FF2E5E8A', '#FF8A3E5E', '#FF3E8A5E', '#FF8A6E2E', '#FF5E3E8A'}

-- Turns a list of names into the items of a picker.
function Transitions.items(names, noun)
    local items = {}
    for index, name in ipairs(names) do
        items[index] = {id = name, text = noun .. ' "' .. name .. '"'}
    end
    return items
end

-- Turns an effect name such as `crossFade` into the words of its button.
function Transitions.label(name)
    return (name:gsub('%u', function(letter) return ' ' .. letter:lower() end):gsub('^%l', string.upper))
end

function Transitions:init(entry)
    Transitions.super.init(self, entry)
    self.options = {direction = 'left', ease = 'cubicInOut', duration = 0.8, color = '#FF000000'}
    self.played = 0
end

function Transitions:enter()
    local options = self.options
    local buttons = {}
    for index, effect in ipairs(Transitions.effects) do
        buttons[index] = ui.button{id = effect, text = Transitions.label(effect), onClick = function()
            self:play(effect)
        end}
    end
    self:frame{
        hint = 'Pick an effect to push a card with it. The card leaves by itself after a moment, or with Escape or the B button.',
        content = {ui.grid{columns = 4, gap = 12, children = buttons}},
        controls = {
            ui.formField{label = 'Direction', ui.combo{id = 'direction', items = Transitions.items(Transitions.directions, 'Direction'), selected = options.direction, onChange = function(event)
                options.direction = event.value
            end}},
            ui.formField{label = 'Easing', ui.combo{id = 'ease', items = Transitions.items(Transitions.eases, 'Easing'), selected = options.ease, onChange = function(event)
                options.ease = event.value
            end}},
            ui.formField{label = 'Duration in seconds', ui.slider{id = 'duration', min = 0.2, max = 2.5, value = options.duration, showValue = true, onChange = function(event)
                options.duration = event.value
            end}},
            ui.formField{label = 'Color of fades and irises', ui.colorField{id = 'color', value = options.color, alpha = false, onChange = function(event)
                options.color = event.value
            end}},
        },
        focus = 'fade',
    }
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
    scene.push(Card({title = 'Effect "' .. effect .. '"', caption = string.format('Direction "%s", easing "%s", %.1f s', options.direction, options.ease, options.duration), color = color, stay = 1.2, leave = leave}), arrive)
    self:set('status', {text = string.format('Pushed with "%s"   Played %d', effect, self.played)})
end

return Transitions
