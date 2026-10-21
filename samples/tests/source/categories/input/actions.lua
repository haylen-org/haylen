-- Action map: the button, axis and vector actions of the input tests with their bindings and live state, driven by keys, the mouse, gamepads and on-screen touch controls at once, the press threshold, the gamepad that the devices of the map read and prompts for the last device used.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local controls = require('categories.input.controls')
local prompts = require('categories.input.prompts')
local Test = require('harness.test')

local Actions = haylen.class('Actions', Test)

Actions.code = [[
local jump = {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}}
input.loadActions({actions = {jump}})
if input.pressed('jump') then hero:jump() end  -- Every binding feeds the same action.
local dx, dy = input.vector('move')
input.actions().devices = {gamepad = 2}
local device = input.lastDevice()  -- Picks the prompts to show.]]

local kLists = {button = {'bindings'}, axis = {'positive', 'negative'}, vector = {'up', 'down', 'left', 'right', 'bindings'}}
local kPrompts = {
    keyboardMouse = 'You play with the keyboard and the mouse: WASD or the arrows move, Space or a left click jumps, Shift or a right click dashes, E and Q throttle.',
    gamepad = 'You play with a gamepad: the left stick or the d-pad moves, the bottom button jumps, the left one dashes and the triggers throttle.',
    touch = 'You play with touch: the stick moves, and the Jump and Dash buttons jump and dash.',
}
local kIndices = {{id = 'all', text = 'All'}, {id = '1', text = '1'}, {id = '2', text = '2'}, {id = '3', text = '3'}, {id = '4', text = '4'}, {id = '5', text = '5'}, {id = '6', text = '6'}, {id = '7', text = '7'}, {id = '8', text = '8'}}
local kFlash = 0.3

function Actions:enter()
    self:loadActions(controls.actions())
    self.rows = {}
    for _, action in ipairs(controls.defaults().actions) do
        local name = action.name
        local definition = input.actionDefinition(name)
        local bindings = {}
        for _, list in ipairs(kLists[definition.type]) do
            for _, binding in ipairs(definition[list] or {}) do
                bindings[#bindings + 1] = binding
            end
        end
        self.rows[#self.rows + 1] = {name = name, type = definition.type, bindings = bindings, pressed = 0, released = 0}
    end
    self:frame({
        code = Actions.code,
        hint = 'Use any device, or several at once: every binding feeds the same actions.',
        play = true,
        controls = {
            ui.formField{label = 'Press threshold', ui.slider{id = 'threshold', min = 0.1, max = 0.9, step = 0.05, value = 0.5, showValue = true, onChange = function(event)
                input.setPressThreshold(event.value)
            end}},
            ui.formField{label = 'Gamepad the bindings read', ui.segmentedControl{id = 'pad', items = kIndices, selected = 'all', onChange = function(event)
                input.actions().devices = event.value ~= 'all' and {gamepad = tonumber(event.value)} or {}
            end}},
            ui.formField{label = 'Touch controls', ui.toggle{id = 'touchOnly', text = 'Only after a touch', checked = true, onChange = function(event)
                for _, id in ipairs({'stick', 'jump', 'dash'}) do
                    self:set(id, {touchOnly = event.checked})
                end
            end}},
        },
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', mode = 'floating', anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 260, height = 260, touchOnly = true},
            ui.row{anchor = 'bottom', anchorTo = 'parent', margin = {0, 0, 24, 0}, gap = 32,
                ui.touchButton{id = 'jump', action = 'jump', text = 'Jump', touchOnly = true},
                ui.touchButton{id = 'dash', action = 'dash', text = 'Dash', touchOnly = true},
            },
        },
    })
end

function Actions:exit()
    Actions.super.exit(self)
    input.setPressThreshold(0.5)
    input.actions().devices = {}
    input.clearVirtual()
end

function Actions:update(dt)
    Actions.super.update(self, dt)
    for _, row in ipairs(self.rows) do
        row.pressed = input.pressed(row.name) and kFlash or math.max(0, row.pressed - dt)
        row.released = input.released(row.name) and kFlash or math.max(0, row.released - dt)
    end
    local x, y = input.vector('move')
    self:status(string.format('The last device is "%s". Move %+.2f, %+.2f, jump %s, dash %s, throttle %+.2f.', input.lastDevice(), x, y, input.down('jump') and 'down' or 'up', input.down('dash') and 'down' or 'up', input.value('throttle')))
end

function Actions:drawRow(row, rect)
    local x, top, width, height = rect[1], rect[2], rect[3], rect[4]
    local down = input.down(row.name)
    local middle = top + height / 2
    local flash = row.pressed > 0 and Test.warm or (row.released > 0 and Test.red or nil)
    graphics2d.drawShape(rect, {radius = 18, color = Test.surface, borderWidth = flash and 3 or 2, borderColor = flash or Test.line})
    graphics2d.drawText(prompts.font(), row.name:sub(1, 1):upper() .. row.name:sub(2), x + 24, middle - 14, {size = 32, anchor = {0, 0.5}, outlineWidth = 3, outlineColor = prompts.ink, layer = 2})
    graphics2d.drawText(nil, row.type:sub(1, 1):upper() .. row.type:sub(2) .. ' action', x + 24, middle + 22, {size = 20, color = Test.accent, anchor = {0, 0.5}, layer = 2})
    local stateX = x + width - 300
    prompts.chips(row.bindings, x + 190, top + 30, 36, stateX - 32)

    -- The light shows the action held, and its rim flashes on a press or a release.
    graphics2d.drawShape({stateX - 22, middle - 34, 44, 44}, {kind = 'ellipse', color = down and Test.green or '#FF15172A', borderWidth = 3, borderColor = flash or Test.line, layer = 2})
    graphics2d.drawText(nil, down and 'Down' or 'Up', stateX, middle + 26, {size = 20, color = down and Test.green or Test.muted, anchor = {0.5, 0.5}, layer = 2})

    -- Axes run from -1 to 1 around the middle of the groove, and the other types from 0 to 1.
    local value = input.value(row.name)
    local barX, barWidth, barHeight = stateX + 50, row.type == 'vector' and 120 or 220, 20
    local axis = row.type == 'axis'
    local zero = axis and barX + barWidth / 2 or barX
    local scale = axis and barWidth / 2 or barWidth
    graphics2d.drawShape({barX, middle - 20, barWidth, barHeight}, {radius = barHeight / 2, color = '#FF15172A', layer = 2})
    if math.abs(value) > 0.001 then
        local fill = math.max(barHeight - 6, math.abs(value) * scale - 6)
        local left = value < 0 and zero - fill - 3 or zero + 3
        graphics2d.drawShape({axis and left or barX + 3, middle - 17, fill, barHeight - 6}, {radius = (barHeight - 6) / 2, color = Test.accent, layer = 3})
    end
    graphics2d.drawText(nil, string.format('Value %+.2f', value), barX, middle + 26, {size = 20, color = Test.muted, anchor = {0, 0.5}, layer = 2})

    if row.type == 'vector' then
        local cx, radius = x + width - 64, math.min(40, height / 2 - 12)
        local vx, vy = input.vector(row.name)
        graphics2d.drawShape({cx - radius, middle - radius, radius * 2, radius * 2}, {kind = 'ellipse', color = '#FF15172A', borderWidth = 3, borderColor = Test.line, layer = 2})
        graphics2d.drawLine(cx, middle, cx + vx * radius, middle + vy * radius, 5, Test.green, {layer = 3})
        graphics2d.drawCircle(cx + vx * radius, middle + vy * radius, 10, Test.green, {layer = 4})
    end
end

function Actions:draw(area)
    local margin, gap = 24, 12
    local promptHeight = 64
    local height = math.min(130, (area.height - margin * 2 - promptHeight - gap * #self.rows) / #self.rows)
    for index, row in ipairs(self.rows) do
        self:drawRow(row, {margin, margin + (index - 1) * (height + gap), area.width - margin * 2, height})
    end
    local top = margin + #self.rows * (height + gap)
    graphics2d.drawText(nil, kPrompts[input.lastDevice()], margin, top + 18, {size = 24, color = Test.ink, maxWidth = area.width - margin * 2, layer = 2})
end

return Actions
