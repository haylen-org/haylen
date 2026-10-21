-- Players: a seat for the keyboard and the touch controls and one for each of the eight gamepads, each with an action map that reads only its own devices. A gamepad joins its seat with its start or south button, the seat lights with its own presses, and a seat whose gamepad disconnects waits for it.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local controls = require('categories.input.controls')
local prompts = require('categories.input.prompts')
local Test = require('harness.test')

local Players = haylen.class('Players', Test)

Players.code = [[
local seat = input.newActionMap({keyboard = false, mouse = false, virtual = false, gamepad = 2, actions = document})
local pad = input.findGamepadPress('start')  -- The gamepad that pressed it, or `nil`.
local dx, dy = seat:vector('move')
local mask = seat:stepButtons({'jump', 'dash'})  -- In `fixedUpdate`.]]

local kSeats = 9
local kButtons = {'jump', 'dash'}
-- The color of each seat, the ring its player stands on and the rim of its card.
local kColors = {'#FF8FB0FF', '#FFF2B23A', '#FF6FDCA0', '#FFFF8A84', '#FFC9A0FF', '#FF5ED3E8', '#FFFFD166', '#FFB8E986', '#FFFF9ED8'}
-- The walker sheet has cells of 256 pixels in eight columns, the idle frames in the first row and the run in the second.
local kIdleFrames = {0, 1, 2, 3}
local kRunFrames = {8, 9, 10, 11, 12, 13, 14, 15}
local kFeet = 248 / 256

function Players:enter()
    self:loadActions(controls.actions())
    local document = controls.actions()
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
    self.icons = {keyboard = assets.texture('input/keyboard.png', {filter = 'linear'}), gamepad = assets.texture('input/gamepad.png', {filter = 'linear'})}
    self.time = 0
    self.seats = {{title = 'Keyboard and touch', map = input.newActionMap({gamepad = false, virtualPrefix = 'seat.', actions = document}), joined = true, x = 0, y = 0, facing = 1, moving = false}}
    for pad = 1, kSeats - 1 do
        self.seats[#self.seats + 1] = {title = 'Gamepad ' .. pad, pad = pad, map = input.newActionMap({keyboard = false, mouse = false, virtual = false, gamepad = pad, actions = document}), joined = false, x = 0, y = 0, facing = 1, moving = false}
    end
    self:listen('gamepadDisconnected', function(event)
        local seat = self.seats[event.gamepad + 1]
        if seat and seat.joined then
            seat.waiting = true
        end
    end)
    self:listen('gamepadConnected', function(event)
        local seat = self.seats[event.gamepad + 1]
        if seat then
            seat.waiting = false
        end
    end)
    self:frame({
        code = Players.code,
        hint = 'Press start or south on a gamepad to join its seat. Each seat moves with its own devices only.',
        play = true,
        overlay = {
            ui.touchStick{action = 'seat.move', mode = 'floating', anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 24, 24}, width = 240, height = 240, touchOnly = true},
            ui.touchButton{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 24, 24, 0}, action = 'seat.jump', text = 'Jump', touchOnly = true},
        },
    })
end

function Players:exit()
    Players.super.exit(self)
    self.seats = nil
    input.clearVirtual()
end

function Players:update(dt)
    Players.super.update(self, dt)
    local pad = input.findGamepadPress('start') or input.findGamepadPress('south')
    local joining = pad and self.seats[pad + 1]
    if joining and not joining.joined then
        joining.joined = true
        self:log('Gamepad %d joined.', pad)
    end
    self.time = self.time + dt
    local joined = 0
    for _, seat in ipairs(self.seats) do
        local dx, dy = seat.map:vector('move')
        seat.x = math.max(-1, math.min(1, seat.x + dx * dt))
        seat.y = math.max(-1, math.min(1, seat.y + dy * dt))
        seat.moving = math.abs(dx) + math.abs(dy) > 0.1
        if math.abs(dx) > 0.1 then
            seat.facing = dx > 0 and 1 or -1
        end
        joined = joined + (seat.joined and 1 or 0)
    end
    self:status(string.format('%d of %d seats are taken. Each moves only with its own devices.', joined, kSeats))
end

function Players:fixedUpdate(step)
    for _, seat in ipairs(self.seats) do
        seat.mask = seat.map:stepButtons(kButtons)
    end
end

function Players:drawSeat(index, seat, rect)
    local x, y, width, height = rect[1], rect[2], rect[3], rect[4]
    local color = kColors[index]
    local waiting = seat.waiting
    graphics2d.drawShape(rect, {radius = 18, color = seat.joined and '#FF1F2433' or '#FF1A1E28', borderWidth = seat.joined and 3 or 2, borderColor = waiting and Test.red or (seat.joined and color or Test.line)})
    graphics2d.drawShape({x + 16, y + 16, 34, 34}, {kind = 'ellipse', color = seat.joined and color or '#FF3A4258', borderWidth = 2, borderColor = prompts.ink, layer = 2})
    graphics2d.drawText(prompts.font(), tostring(index), x + 33, y + 33, {size = 20, color = prompts.ink, anchor = {0.5, 0.5}, layer = 3})
    graphics2d.drawText(prompts.font(), seat.title, x + 62, y + 33, {size = 24, anchor = {0, 0.5}, color = seat.joined and '#FFFFFFFF' or Test.muted, layer = 2})
    if not seat.joined or waiting then
        local icon = self.icons[seat.pad and 'gamepad' or 'keyboard']
        local size = math.min(width * 0.36, (height - 110) * icon.width / icon.height)
        graphics2d.draw(icon, x + width / 2, y + height * 0.52, {width = size, height = size * icon.height / icon.width, color = waiting and '#40FF8A84' or '#30FFFFFF', layer = 1})
        graphics2d.drawText(nil, waiting and 'Waiting for its gamepad' or 'Press Start to join', x + width / 2, y + height - 26, {size = 22, color = waiting and Test.red or Test.muted, anchor = {0.5, 0.5}, layer = 2})
        return
    end

    -- The player stands on a ring of its color and walks inside its seat with its own devices.
    local jump = (seat.mask or 0) & 1 ~= 0
    local cx, feet = x + width * 0.6 + seat.x * width * 0.3, y + height - 24 + seat.y * 8
    local size = math.min(height - 40, 150)
    graphics2d.drawShape({cx - size * 0.3, feet - size * 0.06, size * 0.6, size * 0.12}, {kind = 'ellipse', color = color, borderWidth = 2, borderColor = prompts.ink, layer = 2})
    local frames = seat.moving and kRunFrames or kIdleFrames
    local frame = frames[math.floor(self.time * (seat.moving and 14 or 6)) % #frames + 1]
    local dash = seat.map:down('dash')
    graphics2d.draw(self.walker, cx, feet - (jump and size * 0.2 or 0), {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = size, height = size, pivotY = kFeet, flipHorizontal = seat.facing < 0, flash = dash and '#66FFFFFF' or nil, layer = 3})
end

function Players:draw(area)
    local columns, margin, gap = 3, 24, 16
    local width = (area.width - margin * 2 - (columns - 1) * gap) / columns
    local height = (area.height - margin * 2 - 2 * gap) / 3
    for index, seat in ipairs(self.seats) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        self:drawSeat(index, seat, {margin + column * (width + gap), margin + row * (height + gap), width, height})
    end
end

return Players
