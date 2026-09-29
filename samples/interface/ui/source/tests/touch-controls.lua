-- Touch controls: a touch stick and touch buttons drive virtual controls that actions of the action map read, next to keys and a gamepad, so gameplay code reads one action for every device.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local TouchControls = haylen.class('TouchControls', sample.Test)

TouchControls.hints = 'Drag the stick with a finger or the mouse, or use WASD or the left stick of a gamepad. Jump with the round button, Space or the south button, and dash with the other button or the west button. Every control follows its own finger.'

local kSpeed = 520
local kActions = {
    {name = 'demoMove', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'demoJump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'demoDash', type = 'button', bindings = {'key:leftShift', 'button:west', 'virtual:dash'}},
}

function TouchControls:init(entry)
    TouchControls.super.init(self, entry)
    self.ball = {x = 960, y = 560, height = 0, lift = 0, dash = 0}
    self.last = 'nothing pressed yet'
end

function TouchControls:enter()
    for _, action in ipairs(kActions) do
        input.defineAction(action)
    end
    TouchControls.super.enter(self)
end

function TouchControls:exit()
    for _, action in ipairs(kActions) do
        input.removeAction(action.name)
    end
end

function TouchControls:content()
    return ui.column{
        ui.spacer{grow = 1},
        ui.row{align = 'stretch',
            ui.touchStick{id = 'stick', action = 'move', radius = 130, width = 520, height = 320, align = 'end'},
            ui.spacer{grow = 1},
            ui.row{gap = 32, align = 'end',
                ui.touchButton{action = 'dash', text = 'Dash', size = 130, onPress = function() self.last = 'dash pressed' end},
                ui.touchButton{action = 'jump', image = 'icons/star.png', size = 170, onPress = function() self.last = 'jump pressed' end, onRelease = function() self.last = 'jump released' end},
            },
        },
    }
end

-- Space and the south button jump here, so no control keeps the focus that accept would press.
function TouchControls:started()
    ui.clearFocus()
end

function TouchControls:update(dt)
    local ball = self.ball
    local x, y = input.vector('demoMove')
    if input.pressed('demoDash') then
        ball.dash = 0.25
    end
    ball.dash = math.max(0, ball.dash - dt)
    local speed = kSpeed * (ball.dash > 0 and 3 or 1)
    local area = viewport.safeRect()
    ball.x = m.clamp(ball.x + x * speed * dt, area.x + 60, area:right() - 60)
    ball.y = m.clamp(ball.y + y * speed * dt, area.y + 220, area:bottom() - 120)
    if input.pressed('demoJump') and ball.height == 0 then
        ball.lift = 900
    end
    ball.height = math.max(0, ball.height + ball.lift * dt)
    ball.lift = ball.height > 0 and ball.lift - 2600 * dt or 0
    self:setStatus(string.format('move %.2f, %.2f from %s, %s', x, y, input.lastDevice(), self.last))
end

function TouchControls:render()
    local ball = self.ball
    graphics2d.beginScreen()
    graphics2d.drawCircle(ball.x, ball.y + 36, 40, '#50000000')
    graphics2d.drawCircle(ball.x, ball.y - ball.height, 40, ball.dash > 0 and '#FFFFB74D' or '#FF4FC3F7')
    graphics2d.drawRing(ball.x, ball.y - ball.height, 40, 4, '#FFFFFFFF')
end

return TouchControls
