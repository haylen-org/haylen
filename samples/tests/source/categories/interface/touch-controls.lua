-- Two touch sticks and touch buttons drive virtual controls that actions of the action map read, next to keys and a gamepad, so gameplay code reads one action for every device. The stick that moves follows the finger and draws with images, and the stick that aims stays in place and draws with circles.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local TouchControls = haylen.class('TouchControls', Test)

TouchControls.speed = 520
TouchControls.radius = 40
TouchControls.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'dash', type = 'button', bindings = {'key:leftShift', 'button:west', 'virtual:dash'}},
    {name = 'aim', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'stick:right', 'virtualStick:aim'}},
}}
TouchControls.art = {
    stickBase = {image = 'interface/touch/stick_base.png', filter = 'linear'},
    stickKnob = {image = 'interface/touch/stick_knob.png', filter = 'linear'},
    touchButton = {image = 'interface/touch/touch_button.png', filter = 'linear'},
    touchButtonPressed = {image = 'interface/touch/touch_button_pressed.png', filter = 'linear'},
}

function TouchControls:init(entry)
    TouchControls.super.init(self, entry)
    self.ball = {height = 0, lift = 0, dash = 0, aimX = 1, aimY = 0}
    self.last = 'Nothing pressed yet.'
end

function TouchControls:enter()
    self:loadActions(TouchControls.actions)
    self:frame{
        hint = 'Move with the left stick, which follows a finger that leaves its ring, or with WASD or the left stick of a gamepad, and aim with the right stick, the arrows or the right stick of a gamepad. Jump with the round button, Space or the south button, and dash with the other button, Shift or the west button. Every control follows its own finger.',
        play = true,
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', mode = 'following', radius = 130, width = 520, height = 320, anchor = 'bottomLeft', margin = {0, 0, 150, 40}, style = {surfaces = TouchControls.art}},
            ui.row{anchor = 'bottomRight', margin = {0, 60, 170, 0}, gap = 32, align = 'end',
                ui.touchStick{id = 'aim', action = 'aim', radius = 100, deadZone = 0.25},
                ui.column{gap = 24, style = {surfaces = TouchControls.art},
                    ui.touchButton{action = 'dash', text = 'Dash', size = 130, onPress = function() self.last = 'Dash pressed.' end},
                    ui.touchButton{action = 'jump', image = 'interface/icons/star.png', size = 170, onPress = function() self.last = 'Jump pressed.' end, onRelease = function() self.last = 'Jump released.' end},
                },
            },
        },
    }
end

function TouchControls:resize(area)
    self.ball.x = self.ball.x and m.clamp(self.ball.x, 0, area.width) or area.width / 2
    self.ball.y = self.ball.y and m.clamp(self.ball.y, 0, area.height) or area.height / 2
end

function TouchControls:update(dt)
    TouchControls.super.update(self, dt)
    local ball, area = self.ball, self.area
    if not area then
        return
    end
    local x, y = input.vector('move')
    local aimX, aimY = input.vector('aim')
    if aimX ~= 0 or aimY ~= 0 then
        ball.aimX, ball.aimY = aimX, aimY
    end
    if input.pressed('dash') then
        ball.dash = 0.25
    end
    ball.dash = math.max(0, ball.dash - dt)
    local speed = TouchControls.speed * (ball.dash > 0 and 3 or 1)
    local radius = TouchControls.radius
    ball.x = m.clamp(ball.x + x * speed * dt, radius, area.width - radius)
    ball.y = m.clamp(ball.y + y * speed * dt, radius * 3, area.height - radius)
    if input.pressed('jump') and ball.height == 0 then
        ball.lift = 900
    end
    ball.height = math.max(0, ball.height + ball.lift * dt)
    ball.lift = ball.height > 0 and ball.lift - 2600 * dt or 0
    self:status(string.format('Move %.2f, %.2f and aim %.2f, %.2f from "%s". %s', x, y, aimX, aimY, input.lastDevice(), self.last))
end

function TouchControls:draw(area)
    local ball, radius = self.ball, TouchControls.radius
    graphics2d.drawCircle(ball.x, ball.y + 36, radius, '#50000000')
    graphics2d.drawCircle(ball.x, ball.y - ball.height, radius, ball.dash > 0 and '#FFFFB74D' or '#FF4FC3F7')
    graphics2d.drawRing(ball.x, ball.y - ball.height, radius, 4, '#FFFFFFFF')
    local length = math.sqrt(ball.aimX * ball.aimX + ball.aimY * ball.aimY)
    local tipX, tipY = ball.x + ball.aimX / length * radius * 2, ball.y - ball.height + ball.aimY / length * radius * 2
    graphics2d.drawLine(ball.x, ball.y - ball.height, tipX, tipY, 6, '#FFFFB74D')
end

return TouchControls
