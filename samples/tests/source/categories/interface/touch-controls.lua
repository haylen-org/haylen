-- A touch stick and touch buttons drive virtual controls that actions of the action map read, next to keys and a gamepad, so gameplay code reads one action for every device.
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
}}

function TouchControls:init(entry)
    TouchControls.super.init(self, entry)
    self.ball = {height = 0, lift = 0, dash = 0}
    self.last = 'Nothing pressed yet.'
end

function TouchControls:enter()
    self:loadActions(TouchControls.actions)
    self:frame{
        hint = 'Drag the stick with a finger or the mouse, or use WASD or the left stick of a gamepad. Jump with the round button, Space or the south button, and dash with the other button, Shift or the west button. Every control follows its own finger.',
        play = true,
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', radius = 130, width = 520, height = 320, anchor = 'bottomLeft', margin = {0, 0, 150, 40}},
            ui.row{anchor = 'bottomRight', margin = {0, 60, 170, 0}, gap = 32,
                ui.touchButton{action = 'dash', text = 'Dash', size = 130, onPress = function() self.last = 'Dash pressed.' end},
                ui.touchButton{action = 'jump', image = 'interface/icons/star.png', size = 170, onPress = function() self.last = 'Jump pressed.' end, onRelease = function() self.last = 'Jump released.' end},
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
    self:status(string.format('Move %.2f, %.2f from "%s". %s', x, y, input.lastDevice(), self.last))
end

function TouchControls:draw(area)
    local ball, radius = self.ball, TouchControls.radius
    graphics2d.drawCircle(ball.x, ball.y + 36, radius, '#50000000')
    graphics2d.drawCircle(ball.x, ball.y - ball.height, radius, ball.dash > 0 and '#FFFFB74D' or '#FF4FC3F7')
    graphics2d.drawRing(ball.x, ball.y - ball.height, radius, 4, '#FFFFFFFF')
end

return TouchControls
