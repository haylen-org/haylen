-- Game controls: steppers of numbers and options, segmented controls, range sliders and key capture fields that rebind an action of the action map.
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local sample = require('sample')

local GameControls = haylen.class('GameControls', sample.Test)

GameControls.hints = 'Left and right change a focused stepper, segmented control or range slider, and Enter or the south button moves on or switches the knob. Press a binding field, then a key, a mouse button or a gamepad button to rebind Jump, and press it to see the action fire. Escape cancels the capture.'
GameControls.focus = 'players'

GameControls.bindings = {
    {id = 'jump', action = 'demoJump', label = 'Jump', value = 'key:space', sources = {'key', 'mouse', 'button'}},
    {id = 'dash', action = 'demoDash', label = 'Dash', value = 'button:west', sources = {'button', 'axis'}},
    {id = 'pause', action = 'demoPause', label = 'Pause', value = 'key:p', sources = {'key'}},
}

function GameControls:enter()
    for _, binding in ipairs(GameControls.bindings) do
        input.defineAction({name = binding.action, type = 'button', bindings = {binding.value}})
    end
    GameControls.super.enter(self)
end

function GameControls:exit()
    for _, binding in ipairs(GameControls.bindings) do
        input.removeAction(binding.action)
    end
end

function GameControls:report(name)
    return function(event)
        if event.low then
            self:setStatus(string.format('%s from %g to %g', name, event.low, event.high))
        else
            self:setStatus(name .. ' changed to ' .. (type(event.value) == 'number' and string.format('%g', event.value) or event.value))
        end
    end
end

function GameControls:captureRows()
    local rows = {}
    for index, binding in ipairs(GameControls.bindings) do
        rows[index] = ui.settingsRow{label = binding.label, caption = 'takes ' .. table.concat(binding.sources, ', '),
            ui.keyCapture{id = 'bind-' .. binding.id, value = binding.value, prompt = 'Press a ' .. binding.sources[1], sources = binding.sources, width = 360,
                onChange = function(event)
                    input.defineAction({name = binding.action, type = 'button', bindings = {event.value}})
                    self:setStatus(binding.label .. ' is now ' .. event.value)
                end,
                onCancel = function() self:setStatus('the capture of ' .. binding.label .. ' was cancelled') end,
            },
        }
    end
    return rows
end

function GameControls:content()
    local difficulties = {{id = 'easy', text = 'Easy'}, {id = 'normal', text = 'Normal'}, {id = 'hard', text = 'Hard'}}
    local captures = self:captureRows()
    captures[#captures + 1] = ui.label{id = 'fired', text = 'Press a binding to fire its action.', color = 'textMuted'}
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('stepper', {
                ui.settingsRow{label = 'Players', ui.stepper{id = 'players', value = 2, min = 1, max = 4, width = 360, onChange = self:report('players')}},
                ui.settingsRow{label = 'Difficulty', caption = 'wraps around', ui.stepper{items = difficulties, selected = 'normal', wrap = true, width = 360, onChange = self:report('difficulty')}},
                ui.settingsRow{label = 'Gamma', ui.stepper{value = 1, min = 0.5, max = 2, step = 0.1, decimals = 1, width = 360, onChange = self:report('gamma')}},
            }),
            sample.section('segmentedControl', {
                ui.segmentedControl{items = {{id = 'daily', text = 'Daily'}, {id = 'weekly', text = 'Weekly'}, {id = 'all', text = 'All time'}}, selected = 'daily', onChange = self:report('leaderboard')},
                ui.segmentedControl{items = {{id = 'map', text = 'Map'}, {id = 'quests', text = 'Quests'}, {id = 'crafting', text = 'Crafting', enabled = false}, {id = 'bag', text = 'Bag'}}, selected = 'bag', onChange = self:report('view')},
            }),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('rangeSlider', {
                ui.label{text = 'Price filter in steps of 10'},
                ui.rangeSlider{min = 0, max = 500, low = 50, high = 300, step = 10, showValue = true, decimals = 0, onChange = self:report('price')},
                ui.label{text = 'Free range with two decimals'},
                ui.rangeSlider{low = 0.2, high = 0.6, showValue = true, onChange = self:report('range')},
            }),
            sample.section('keyCapture', captures),
        },
    }
end

function GameControls:update(dt)
    for _, binding in ipairs(GameControls.bindings) do
        if input.pressed(binding.action) then
            self.document:set('fired', {text = binding.label .. ' fired at ' .. string.format('%.1f', haylen.elapsed()) .. ' s'})
        end
    end
end

return GameControls
