-- Steppers of numbers and options, segmented controls, range sliders and key capture fields that rebind actions of the action map, which the play area reads as game input.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local GameControls = haylen.class('GameControls', Test)

GameControls.code = [[
ui.stepper{value = 2, min = 1, max = 4, onChange = function(event) players = event.value end}
ui.segmentedControl{items = {{id = 'daily', text = 'Daily'}, {id = 'weekly', text = 'Weekly'}}, selected = 'daily'}
ui.rangeSlider{min = 0, max = 500, low = 50, high = 300, step = 10, onChange = function(event) filter(event.low, event.high) end}
ui.keyCapture{value = 'key:space', sources = {'key', 'mouse', 'button'}, onChange = function(event)
    input.defineAction({name = 'jump', type = 'button', bindings = {event.value}})
end}]]

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
        code = GameControls.code,
        focus = 'players',
        panelWidth = 820,
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
                    self:set('status', {text = string.format('%s is now bound to %s.', binding.label, GameControls.bindingName(event.value))})
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

-- Names a binding the way a player reads it, such as "Space" for "key:space" or "West button" for "button:west".
function GameControls.bindingName(value)
    local source, name = value:match('^(%a+):(.+)$')
    local words = name:gsub('(%l)(%u)', '%1 %2'):lower()
    words = words:sub(1, 1):upper() .. words:sub(2)
    if source == 'button' then
        return words .. ' button'
    elseif source == 'mouse' then
        return words .. ' mouse button'
    elseif source == 'axis' then
        return words .. ' axis'
    end
    return #words == 1 and words:upper() or words
end

-- One key cap per action, pressed while the action is down and glowing for a moment after it fires, with the action, its binding and how often it fired.
function GameControls:draw(area)
    local count = #GameControls.bindings
    local rowHeight = math.min(150, (area.height - 40) / count)
    local top = (area.height - rowHeight * count) / 2
    local now = haylen.elapsed()
    for index, binding in ipairs(GameControls.bindings) do
        local fired = self.fired[binding.action]
        local name = GameControls.bindingName(fired.value)
        local down = input.down(binding.action)
        local y = top + (index - 1) * rowHeight + rowHeight / 2
        local width = math.max(110, graphics2d.measureText(nil, name, {size = 30}) + 48)
        local x = 56
        local glow = fired.at and math.max(0, 1 - (now - fired.at) / 0.5) or 0
        local sink = down and 5 or 0
        if glow > 0 then
            graphics2d.drawShape({x - 14, y - 50, width + 28, 100}, {radius = 26, color = {0.44, 0.86, 0.63, 0.55 * glow}, softness = 24})
        end
        graphics2d.drawShape({x, y - 34 + 8, width, 68}, {radius = 16, color = '#FF141826'})
        graphics2d.drawShape({x, y - 34 + sink, width, 68}, {radius = 16, color = down and '#FF3DBE7A' or '#FF39405C', borderWidth = 3, borderColor = '#FF15172A'})
        graphics2d.drawShape({x + 8, y - 28 + sink, width - 16, 24}, {radius = 10, color = down and '#4DFFFFFF' or '#26FFFFFF'})
        graphics2d.drawText(nil, name, x + width / 2, y + sink, {size = 30, color = Test.ink, anchor = {0.5, 0.5}})
        local textX = x + width + 32
        Test.caption(binding.label, textX, y - 38, {size = 32, color = Test.ink})
        local times = fired.count == 1 and 'once' or (fired.count == 2 and 'twice' or fired.count .. ' times')
        Test.caption(fired.count == 0 and 'Press ' .. name .. ' to fire it' or 'Fired ' .. times, textX, y + 6, {size = 26, color = fired.count == 0 and Test.muted or Test.green, maxWidth = area.width - textX - 24})
    end
end

return GameControls
