-- Commands: a command buffer that types runs, a jump attack and three specials from the step state of seven actions, with touch buttons that a sliding thumb presses in turn, with the kept command, its age and the typed history, a facing that swaps forward and back, and a freeze that stops lifetimes as hit stop does.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local Test = require('harness.test')
local prompts = require('categories.input.prompts')

local Commands = haylen.class('Commands', Test)

Commands.code = [[
local run = {name = 'run', steps = {'forward', 'forward'}, maxTicks = 9, priority = 1}
local special = {name = 'forwardSpecial', steps = {'defend', 'forward', 'attack'}, priority = 3}
local directions = {back = 'left', forward = 'right'}
local buffer = input.newCommandBuffer({buttons = names, lifetime = 10, directions = directions, commands = {run, special}})
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
-- The commands as the legend lists them, with the steps the player types.
local kCommands = {
    {name = 'run', title = 'Run', steps = {'forward', 'forward'}},
    {name = 'jumpAttack', title = 'Jump attack', steps = {'jump', 'attack'}},
    {name = 'forwardSpecial', title = 'Forward special', steps = {'defend', 'forward', 'attack'}},
    {name = 'upSpecial', title = 'Up special', steps = {'defend', 'up', 'attack'}},
    {name = 'downSpecial', title = 'Down special', steps = {'defend', 'down', 'jump'}},
}
-- The angle of the arrow of each direction, and the face button each button takes on a gamepad.
local kArrows = {up = -math.pi / 2, down = math.pi / 2, left = math.pi, right = 0}
local kFaces = {defend = 'north', jump = 'south', attack = 'west'}
-- The walker sheet has cells of 256 pixels in eight columns: idle frames in the first row, the run in the second and the swing of the sword in the third.
local kIdleFrames = {0, 1, 2, 3}
local kRunFrames = {8, 9, 10, 11, 12, 13, 14, 15}
local kAttackFrames = {16, 17, 18, 19, 20, 21}
local kFeet = 248 / 256

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
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.time = 0
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
    self.time = self.time + dt
    self:status(string.format('Step %d of the buffer, facing %s, with its lifetimes %s.', self.buffer.tick, self.buffer.facing > 0 and 'right' or 'left', self.buffer.frozen and 'frozen' or 'counting'))
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

-- Draws the chip of a step with its left edge at `x` and its middle at `y`, and returns its width: a key with an arrow for a direction, which forward and back turn with the facing, or a pill in the color of the face button of a gamepad with the name of a button.
function Commands:drawStep(step, x, y, size, lit, layer)
    local direction = step
    if step == 'forward' or step == 'back' then
        direction = (step == 'forward') == (self.buffer.facing > 0) and 'right' or 'left'
    end
    local angle = kArrows[direction]
    local radius = size / 2
    if angle then
        local cx = x + radius
        graphics2d.drawShape({x, y - radius, size, size}, {radius = size * 0.24, color = lit and Test.accent or '#FFE6E8F5', borderWidth = 2, borderColor = prompts.ink, layer = layer})
        local points = {}
        for index, corner in ipairs({{0.55, 0}, {-0.4, -0.5}, {-0.4, 0.5}}) do
            local cos, sin = math.cos(angle), math.sin(angle)
            points[index] = {cx + (corner[1] * cos - corner[2] * sin) * radius * 0.8, y + (corner[1] * sin + corner[2] * cos) * radius * 0.8}
        end
        graphics2d.drawPolygon(points, prompts.ink, {layer = layer + 1})
        return size
    end
    local text = step:sub(1, 1):upper() .. step:sub(2)
    local width = graphics2d.measureText(prompts.font(), text, {size = size * 0.48}) + size * 0.8
    graphics2d.drawShape({x, y - radius, width, size}, {radius = radius, color = prompts.faceColors[kFaces[step]], borderWidth = lit and 4 or 2, borderColor = lit and '#FFFFFFFF' or prompts.ink, layer = layer})
    graphics2d.drawText(prompts.font(), text, x + width / 2, y, {size = size * 0.48, color = '#FFFFFFFF', anchor = {0.5, 0.5}, outlineWidth = 2, outlineColor = prompts.ink, layer = layer + 1})
    return width
end

function Commands:walkerFrame()
    local shown = self.shown
    if shown == 'run' then
        return kRunFrames[math.floor(self.time * 14) % #kRunFrames + 1]
    elseif shown then
        return kAttackFrames[math.min(#kAttackFrames, math.floor(self.age / 2) + 1)]
    end
    return kIdleFrames[math.floor(self.time * 6) % #kIdleFrames + 1]
end

function Commands:draw(area)
    local margin, gap = 24, 16
    local top = margin
    local width = (area.width - margin * 2 - gap) * 0.5
    local height = math.min(300, area.height - 230)

    -- The kept command, with the walker acting it out.
    local kept = {margin, top, width, height}
    graphics2d.drawShape(kept, {radius = 18, color = Test.surface, borderWidth = self.shown and 3 or 2, borderColor = self.shown and Test.warm or Test.line})
    graphics2d.drawText(nil, 'Kept command', margin + 24, top + 22, {size = 24, color = Test.muted, layer = 2})
    local title = 'None'
    for _, command in ipairs(kCommands) do
        if command.name == self.shown then
            title = command.title
        end
    end
    graphics2d.drawText(prompts.font(), title, margin + 24, top + 100, {size = 56, color = self.shown and Test.warm or Test.muted, anchor = {0, 0.5}, outlineWidth = 4, outlineColor = prompts.ink, maxWidth = width - 260, wrap = false, minScale = 0.6, layer = 2})
    if self.shown then
        graphics2d.drawText(nil, string.format('Kept for %d steps', self.age), margin + 24, top + 160, {size = 24, color = Test.ink, layer = 2})
    end
    local frame = self:walkerFrame()
    local size = math.min(220, height - 30)
    graphics2d.drawShape({margin + width - 190, top + height - 34, 140, 24}, {kind = 'ellipse', color = '#55000000', layer = 2})
    graphics2d.draw(self.walker, margin + width - 120, top + height - 22, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = size, height = size, pivotY = kFeet, flipHorizontal = self.buffer.facing < 0, layer = 3})

    -- The commands the buffer knows, the kept one lit.
    local left = margin + width + gap
    local list = {left, top, width, height}
    graphics2d.drawShape(list, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    graphics2d.drawText(nil, 'Commands', left + 24, top + 22, {size = 24, color = Test.muted, layer = 2})
    local row = math.min(52, (height - 70) / #kCommands)
    for index, command in ipairs(kCommands) do
        local y = top + 70 + (index - 0.5) * row
        local lit = command.name == self.shown
        if lit then
            graphics2d.drawShape({left + 12, y - row / 2 + 2, width - 24, row - 4}, {radius = 12, color = '#33FFC93C', layer = 1})
        end
        graphics2d.drawText(nil, command.title, left + 24, y, {size = 24, color = lit and Test.warm or Test.ink, anchor = {0, 0.5}, layer = 2})
        local x = left + width * 0.46
        for _, name in ipairs(command.steps) do
            x = x + self:drawStep(name, x, y, row - 12, false, 2) + 8
        end
    end

    -- The presses of the last steps, newest first.
    local presses = top + height + gap
    graphics2d.drawText(nil, 'Presses, newest first', margin, presses + 16, {size = 24, color = Test.muted, layer = 2})
    local chip = 60
    local x = margin
    for index, name in ipairs(self.history) do
        if x + chip * 2.4 > area.width - margin then
            break
        end
        x = x + self:drawStep(name, x, presses + 56 + chip / 2, chip, index == 1, 2) + 12
    end
    if #self.history == 0 then
        graphics2d.drawText(nil, 'Press a direction or a button to see it here.', margin, presses + 56 + chip / 2, {size = 24, color = Test.muted, anchor = {0, 0.5}, layer = 2})
    end
end

return Commands
