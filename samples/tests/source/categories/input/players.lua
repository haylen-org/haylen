-- Players: a seat for the keyboard and the touch controls and one for each of the eight gamepads, each with an action map that reads only its own devices. A gamepad joins its seat with its start or south button, the seat lights with its own presses, and a seat whose gamepad disconnects waits for it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local controls = require('categories.input.controls')
local Test = require('harness.test')

local Players = haylen.class('Players', Test)

local kSeats = 9
local kButtons = {'jump', 'dash'}

function Players:enter()
    self:loadActions(controls.actions())
    local document = controls.actions()
    self.seats = {{title = 'Keyboard and touch', map = input.newActionMap({gamepad = false, virtualPrefix = 'seat.', actions = document}), joined = true, x = 0, y = 0}}
    for pad = 1, kSeats - 1 do
        self.seats[#self.seats + 1] = {title = 'Gamepad ' .. pad, pad = pad, map = input.newActionMap({keyboard = false, mouse = false, virtual = false, gamepad = pad, actions = document}), joined = false, x = 0, y = 0}
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
        hint = 'Press start or south on a gamepad to join its seat. Each seat moves with its own devices only.',
        play = true,
        controls = {
            ui.sectionTitle{text = 'One map per player'},
            ui.label{font = 'monospace', text = "input.newActionMap({gamepad = 2})\ninput.findGamepadPress('start')\nmap:vector('move')\nmap:stepButtons({'jump', 'dash'})"},
        },
        overlay = {
            ui.touchStick{action = 'seat.move', mode = 'floating', anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 240, height = 240},
            ui.touchButton{anchor = 'bottomRight', margin = {0, 40, 110, 0}, action = 'seat.jump', text = 'Jump'},
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
    for _, seat in ipairs(self.seats) do
        local dx, dy = seat.map:vector('move')
        seat.x = math.max(-1, math.min(1, seat.x + dx * dt))
        seat.y = math.max(-1, math.min(1, seat.y + dy * dt))
    end
end

function Players:fixedUpdate(step)
    for _, seat in ipairs(self.seats) do
        seat.mask = seat.map:stepButtons(kButtons)
    end
end

function Players:draw(area)
    local columns = 3
    local width = (area.width - 48 - (columns - 1) * 16) / columns
    local height = (area.height - 48 - 2 * 16) / 3
    for index, seat in ipairs(self.seats) do
        local column, row = (index - 1) % columns, (index - 1) // columns
        local x, y = 24 + column * (width + 16), 24 + row * (height + 16)
        local color = seat.waiting and Test.red or (seat.joined and Test.green or Test.muted)
        graphics2d.drawRect({x, y, width, height}, Test.surface)
        Test.caption(seat.title, x + 16, y + 12, {color = Test.ink})
        Test.caption(seat.waiting and 'Waiting for its gamepad' or (seat.joined and 'Joined' or 'Press start to join'), x + 16, y + 40, {color = color})
        if seat.joined then
            local cx, cy = x + width * 0.5 + seat.x * width * 0.3, y + height * 0.6 + seat.y * height * 0.2
            local jump = (seat.mask or 0) & 1 ~= 0
            graphics2d.drawCircle(cx, cy, jump and 26 or 20, jump and Test.warm or color, {layer = 1})
            if seat.map:down('dash') then
                graphics2d.drawRing(cx, cy, 32, 4, Test.accent, {layer = 2})
            end
        end
    end
end

return Players
