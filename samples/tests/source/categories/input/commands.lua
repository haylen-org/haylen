-- Commands: a command buffer that types runs, a jump attack and three specials from the step state of seven actions, with touch buttons that a sliding thumb presses in turn, with the kept command, its age and the typed history, a facing that swaps forward and back, and a freeze that stops lifetimes as hit stop does.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')

local Commands = haylen.class('Commands', Test)

Commands.code = [[
local buffer = input.newCommandBuffer({buttons = {'defend', 'up', 'down', 'left', 'right', 'jump', 'attack'}, lifetime = 10, directions = {back = 'left', forward = 'right'}, commands = {
    {name = 'run', steps = {'forward', 'forward'}, maxTicks = 9, priority = 1},
    {name = 'forwardSpecial', steps = {'defend', 'forward', 'attack'}, priority = 3, lifetime = false},
}})
buffer:update(input.actions())  -- Once in every fixed step.
local typed = buffer:peek()]]

local kButtons = {'defend', 'up', 'down', 'left', 'right', 'jump', 'attack'}
local kActions = {actions = {
    {name = 'up', type = 'button', bindings = {'key:w', 'key:up', 'button:dpadUp', 'axis:leftY-', 'virtual:up'}},
    {name = 'down', type = 'button', bindings = {'key:s', 'key:down', 'button:dpadDown', 'axis:leftY+', 'virtual:down'}},
    {name = 'left', type = 'button', bindings = {'key:a', 'key:left', 'button:dpadLeft', 'axis:leftX-', 'virtual:left'}},
    {name = 'right', type = 'button', bindings = {'key:d', 'key:right', 'button:dpadRight', 'axis:leftX+', 'virtual:right'}},
    {name = 'attack', type = 'button', bindings = {'key:j', 'button:west', 'virtual:attack'}},
    {name = 'jump', type = 'button', bindings = {'key:k', 'key:space', 'button:south', 'virtual:jump'}},
    {name = 'defend', type = 'button', bindings = {'key:l', 'button:north', 'virtual:defend'}},
    {name = 'pad', type = 'vector', bindings = {'virtualStick:pad'}},
}}
local kFacings = {{id = '1', text = 'Right'}, {id = '-1', text = 'Left'}}
local kHistory = 12

function Commands:enter()
    self:loadActions(kActions)
    self.buffer = input.newCommandBuffer({
        buttons = kButtons,
        lifetime = 10,
        lifetimes = {up = 1, down = 1, left = 1, right = 1},
        directions = {back = 'left', forward = 'right'},
        commands = {
            {name = 'run', steps = {'forward', 'forward'}, maxTicks = 9, priority = 1},
            {name = 'jumpAttack', steps = {'jump', 'attack'}, maxTicks = 0, priority = 2, clearsHistory = false},
            {name = 'forwardSpecial', steps = {'defend', 'forward', 'attack'}, priority = 3, lifetime = false},
            {name = 'upSpecial', steps = {'defend', 'up', 'attack'}, priority = 4, lifetime = false},
            {name = 'downSpecial', steps = {'defend', 'down', 'jump'}, priority = 4, lifetime = false},
        },
    })
    self.history = {}
    self.shown, self.age = nil, 0
    self:frame({
        code = Commands.code,
        hint = 'Type commands with WASD or the arrows, J, K and L, a gamepad with the d-pad, west, south and north, or the touch buttons. Double tap forward to run, press jump and attack together, or press defend, a direction and attack or jump for the specials.',
        play = true,
        controls = {
            ui.formField{label = 'Facing', ui.segmentedControl{id = 'facing', items = kFacings, selected = '1', onChange = function(event)
                self.buffer.facing = tonumber(event.value)
            end}},
            ui.formField{label = 'Hit stop', ui.toggle{id = 'frozen', text = 'Freeze the lifetimes', onChange = function(event)
                self.buffer.frozen = event.checked
            end}},
            ui.button{id = 'consume', text = 'Consume the kept command', onClick = function()
                local typed = self.buffer:peek()
                if typed then
                    self.buffer:consume(typed)
                end
            end},
        },
        overlay = {
            ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, gap = 16,
                ui.touchButton{action = 'defend', text = 'Defend', size = 100, slideIn = true, touchOnly = true},
                ui.touchButton{action = 'jump', text = 'Jump', size = 100, slideIn = true, touchOnly = true},
                ui.touchButton{action = 'attack', text = 'Attack', size = 100, slideIn = true, touchOnly = true},
            },
            ui.touchStick{action = 'pad', mode = 'floating', anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 240, height = 240, touchOnly = true},
        },
    })
end

function Commands:exit()
    Commands.super.exit(self)
    input.clearVirtual()
end

-- The floating stick drives the four direction buttons, so touch types the same sequences as the keys.
function Commands:update(dt)
    Commands.super.update(self, dt)
    local x, y = input.vector('pad')
    input.setVirtualButton('left', x < -0.5)
    input.setVirtualButton('right', x > 0.5)
    input.setVirtualButton('up', y < -0.5)
    input.setVirtualButton('down', y > 0.5)
    self:status(string.format('Step %d, facing %s, %s', self.buffer.tick, self.buffer.facing > 0 and 'right' or 'left', self.buffer.frozen and 'frozen' or 'counting'))
end

function Commands:fixedUpdate(step)
    local buffer = self.buffer
    local before = buffer.tick
    buffer:update(input.actions())
    for _, name in ipairs(kButtons) do
        if input.stepPressed(name) then
            table.insert(self.history, 1, name)
            self.history[kHistory + 1] = nil
        end
    end
    local typed = buffer:peek()
    self.age = typed == self.shown and self.age + (buffer.tick - before) or 0
    self.shown = typed
end

function Commands:draw(area)
    graphics2d.drawRect({24, 24, area.width - 48, 200}, Test.surface)
    Test.caption('Kept command', 48, 40)
    Test.caption(self.shown and (self.shown:sub(1, 1):upper() .. self.shown:sub(2)) or 'None', 48, 80, {size = 64, color = self.shown and Test.warm or Test.muted})
    Test.caption(string.format('Kept for %d steps', self.age), 48, 170, {color = Test.ink})

    Test.caption('Presses, newest first', 24, 250)
    for index, name in ipairs(self.history) do
        local x = 24 + (index - 1) * 104
        graphics2d.drawRect({x, 284, 96, 56}, index == 1 and Test.accent or Test.line, {layer = 1})
        Test.caption(name:sub(1, 1):upper() .. name:sub(2), x + 48, 300, {anchor = {0.5, 0}, color = Test.ink, size = 17})
    end
end

return Commands
