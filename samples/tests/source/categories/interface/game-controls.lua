-- Steppers of numbers and options, segmented controls, range sliders and key capture fields that rebind actions of the action map, which the play area reads as game input.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local GameControls = haylen.class('GameControls', Test)

GameControls.bindings = {
    {action = 'jump', label = 'Jump', value = 'key:space', sources = {'key', 'mouse', 'button'}},
    {action = 'dash', label = 'Dash', value = 'button:west', sources = {'button', 'axis'}},
    {action = 'pause', label = 'Pause', value = 'key:p', sources = {'key'}},
}

function GameControls:init(entry)
    GameControls.super.init(self, entry)
    self.fired = {}
    for _, binding in ipairs(GameControls.bindings) do
        self.fired[binding.action] = {count = 0, value = binding.value}
    end
end

function GameControls:enter()
    local actions = {}
    for index, binding in ipairs(GameControls.bindings) do
        actions[index] = {name = binding.action, type = 'button', bindings = {binding.value}}
    end
    self:loadActions({actions = actions})
    self:frame{
        hint = 'Left and right change a focused stepper, segmented control or range slider, and Enter or the south button moves on or switches the knob. Press a binding field, then a key, a mouse button or a gamepad button to rebind its action, and press the binding on the play area to see the action fire. Escape cancels a capture.',
        focus = 'players',
        panelWidth = 1060,
        controls = self:controls(),
    }
end

function GameControls:report(name)
    return function(event)
        if event.low then
            self:set('status', {text = string.format('The %s spans %g to %g.', name, event.low, event.high)})
            return
        end
        local value = type(event.value) == 'number' and string.format('%g', event.value) or '"' .. event.value .. '"'
        self:set('status', {text = string.format('The %s changed to %s.', name, value)})
    end
end

function GameControls:captureRows()
    local rows = {}
    for index, binding in ipairs(GameControls.bindings) do
        rows[index] = ui.settingsRow{label = binding.label, caption = 'Takes "' .. table.concat(binding.sources, '", "') .. '"',
            ui.keyCapture{id = 'bind-' .. binding.action, value = binding.value, prompt = 'Press a ' .. binding.sources[1], sources = binding.sources, width = 360,
                onChange = function(event)
                    input.defineAction({name = binding.action, type = 'button', bindings = {event.value}})
                    self.fired[binding.action].value = event.value
                    self:set('status', {text = string.format('%s is now "%s".', binding.label, event.value)})
                end,
                onCancel = function() self:set('status', {text = 'The capture of ' .. binding.label .. ' was cancelled.'}) end,
            },
        }
    end
    return rows
end

function GameControls:controls()
    local difficulties = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}
    return {
        layout.section('Component "stepper"', {
            ui.settingsRow{label = 'Players', ui.stepper{id = 'players', value = 2, min = 1, max = 4, width = 360, onChange = self:report('players')}},
            ui.settingsRow{label = 'Difficulty', caption = 'Wraps around', ui.stepper{items = difficulties, selected = 'normal', wrap = true, width = 360, onChange = self:report('difficulty')}},
            ui.settingsRow{label = 'Gamma', ui.stepper{value = 1, min = 0.5, max = 2, step = 0.1, decimals = 1, width = 360, onChange = self:report('gamma')}},
        }),
        layout.section('Component "segmentedControl"', {
            ui.segmentedControl{items = {{id = 'daily', text = 'Daily'}, {id = 'weekly', text = 'Weekly'}, {id = 'all', text = 'All time'}}, selected = 'daily', onChange = self:report('leaderboard')},
            ui.segmentedControl{items = {{id = 'map', text = 'Map'}, {id = 'quests', text = 'Quests'}, {id = 'crafting', text = 'Crafting', enabled = false}, {id = 'bag', text = 'Bag'}}, selected = 'bag', onChange = self:report('view')},
        }),
        layout.section('Component "rangeSlider"', {
            ui.label{text = 'Price filter in steps of 10'},
            ui.rangeSlider{min = 0, max = 500, low = 50, high = 300, step = 10, showValue = true, decimals = 0, onChange = self:report('price')},
            ui.label{text = 'Free range with two decimals'},
            ui.rangeSlider{low = 0.2, high = 0.6, showValue = true, onChange = self:report('range')},
        }),
        layout.section('Component "keyCapture"', self:captureRows()),
    }
end

function GameControls:update(dt)
    GameControls.super.update(self, dt)
    for _, binding in ipairs(GameControls.bindings) do
        if input.pressed(binding.action) then
            local fired = self.fired[binding.action]
            fired.count = fired.count + 1
            fired.at = haylen.elapsed()
        end
    end
end

-- One lamp per action, lit while the action is down, with its binding and how often it fired.
function GameControls:draw(area)
    Test.caption('Actions read on the play area', 32, 28, {size = 26, color = Test.ink})
    for index, binding in ipairs(GameControls.bindings) do
        local fired = self.fired[binding.action]
        local y = 120 + (index - 1) * 140
        local down = input.down(binding.action)
        graphics2d.drawCircle(80, y + 30, 36, down and Test.green or Test.surface)
        graphics2d.drawRing(80, y + 30, 36, 4, down and Test.ink or Test.line)
        Test.caption(binding.label, 140, y, {size = 30, color = Test.ink})
        local last = fired.at and string.format(', last at %.1f s', fired.at) or ''
        Test.caption(string.format('Bound to "%s", fired %d times%s', fired.value, fired.count, last), 140, y + 40, {size = 21, maxWidth = area.width - 170})
    end
end

return GameControls
