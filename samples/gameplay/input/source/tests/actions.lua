-- Action map: the button, axis and vector actions of the sample with their bindings and live state, driven by keys, the mouse, gamepads and on-screen touch controls at once, the press threshold, the gamepad that bindings read and prompts for the last device used.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')

local Actions = haylen.class('Actions', sample.Test)

local kLists = {button = {'bindings'}, axis = {'positive', 'negative'}, vector = {'up', 'down', 'left', 'right', 'bindings'}}
local kPrompts = {
    keyboard_mouse = 'Keyboard and mouse: WASD or the arrows move, Space or a left click jumps, Left Shift or a right click dashes, E and Q throttle.',
    gamepad = 'Gamepad: the left stick or the d-pad moves, south jumps, west dashes, the triggers throttle.',
    touch = 'Touch: the stick moves, and the Jump and Dash buttons jump and dash.',
}
local kIndices = {{id = 'all', text = 'All'}, {id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}, {id = '4', text = '4'}}
local kFlash = 0.3

function Actions:enter()
    self.rows = {}
    for _, name in ipairs(input.actionNames()) do
        local definition = input.actionDefinition(name)
        local lists = {}
        for _, list in ipairs(kLists[definition.type]) do
            if definition[list] then
                lists[#lists + 1] = list .. ' ' .. table.concat(definition[list], ', ')
            end
        end
        self.rows[#self.rows + 1] = {name = name, type = definition.type, bindings = table.concat(lists, '\n'), pressed = 0, released = 0}
    end
    self:frame({
        hint = 'Use any device, or several at once: every binding feeds the same actions.',
        navigation = not window.hasPointerDevice(),
        focus = 'threshold',
        controls = {
            ui.formField{label = 'Press threshold', ui.slider{id = 'threshold', min = 0.1, max = 0.9, step = 0.05, value = 0.5, showValue = true, onChange = function(event)
                input.setPressThreshold(event.value)
            end}},
            ui.formField{label = 'Gamepad the bindings read', ui.segmentedControl{id = 'pad', items = kIndices, selected = 'all', onChange = function(event)
                input.setGamepadIndex(event.value ~= 'all' and tonumber(event.value) or nil)
            end}},
            ui.toggle{id = 'touchOnly', text = 'Touch controls only after a touch', onChange = function(event)
                for _, id in ipairs({'stick', 'jump', 'dash'}) do
                    self:set(id, {touchOnly = event.checked})
                end
            end},
            ui.sectionTitle{text = 'Reading actions'},
            ui.label{font = 'monospace', text = "input.down('jump')\ninput.pressed('dash')\ninput.value('throttle')\ninput.vector('move')\ninput.lastDevice()"},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', floating = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 260, height = 260},
            ui.row{anchor = 'bottom', margin = {0, 0, 110, 0}, gap = 32,
                ui.touchButton{id = 'jump', action = 'jump', text = 'Jump'},
                ui.touchButton{id = 'dash', action = 'dash', text = 'Dash'},
            },
        },
    })
end

function Actions:exit()
    input.setPressThreshold(0.5)
    input.setGamepadIndex(nil)
    input.clearVirtual()
end

function Actions:update(dt)
    Actions.super.update(self, dt)
    for _, row in ipairs(self.rows) do
        row.pressed = input.pressed(row.name) and kFlash or math.max(0, row.pressed - dt)
        row.released = input.released(row.name) and kFlash or math.max(0, row.released - dt)
    end
    local x, y = input.vector('move')
    self:status(string.format('last device %s   move %+.2f, %+.2f   jump %s   dash %s   throttle %+.2f', input.lastDevice(), x, y, input.down('jump'), input.down('dash'), input.value('throttle')))
end

function Actions:drawRow(row, top, width)
    graphics2d.drawRect({12, top, width - 24, 120}, sample.surface)
    sample.caption(row.name, 32, top + 14, {size = 30, color = sample.ink})
    sample.caption(row.type, 32, top + 56, {color = sample.accent})
    graphics2d.drawText(nil, row.bindings, 200, top + 12, {size = 17, color = sample.muted, maxWidth = width * 0.5, layer = 2})

    local stateX = width * 0.66
    local down = input.down(row.name)
    graphics2d.drawCircle(stateX, top + 50, 24, down and sample.green or '#FF2E3548', {layer = 1})
    if row.pressed > 0 then
        graphics2d.drawRing(stateX, top + 50, 32, 4, sample.warm, {layer = 2})
    elseif row.released > 0 then
        graphics2d.drawRing(stateX, top + 50, 32, 4, sample.red, {layer = 2})
    end
    sample.caption(down and 'down' or 'up', stateX, top + 86, {anchor = {0.5, 0}, size = 18})

    -- Axes run from -1 to 1 around the middle of the bar, and the other types from 0 to 1.
    local value = input.value(row.name)
    local barX, barWidth = width * 0.72, width * 0.14
    local zero = row.type == 'axis' and barX + barWidth / 2 or barX
    local scale = row.type == 'axis' and barWidth / 2 or barWidth
    graphics2d.drawRect({barX, top + 40, barWidth, 22}, '#FF2E3548', {layer = 1})
    graphics2d.drawRect({math.min(zero, zero + value * scale), top + 40, math.abs(value * scale), 22}, sample.accent, {layer = 2})
    sample.caption(string.format('value %+.2f', value), barX, top + 72, {size = 18})

    if row.type == 'vector' then
        local cx, cy, radius = width * 0.92, top + 60, 48
        local x, y = input.vector(row.name)
        graphics2d.drawRing(cx, cy, radius, 3, sample.line, {layer = 1})
        graphics2d.drawLine(cx, cy, cx + x * radius, cy + y * radius, 5, sample.green, {layer = 2})
        graphics2d.drawCircle(cx + x * radius, cy + y * radius, 10, sample.green, {layer = 3})
    end
end

function Actions:draw(area)
    for index, row in ipairs(self.rows) do
        self:drawRow(row, 12 + (index - 1) * 130, area.width)
    end
    local top = 12 + #self.rows * 130 + 8
    sample.caption('Last device: ' .. input.lastDevice(), 24, top, {size = 26, color = sample.warm})
    graphics2d.drawText(nil, kPrompts[input.lastDevice()], 24, top + 40, {size = 22, color = sample.ink, maxWidth = area.width - 48, layer = 2})
end

return Actions
