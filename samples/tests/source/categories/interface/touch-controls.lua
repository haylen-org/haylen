-- Two touch sticks and touch buttons drive virtual controls that actions of the action map read, next to keys and a gamepad, so gameplay code reads one action for every device. The stick that moves follows the finger and draws with images, and the stick that aims stays in place and draws with circles.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local Test = require('harness.test')

local TouchControls = haylen.class('TouchControls', Test)

TouchControls.code = [[
input.defineAction({name = 'move', type = 'vector', bindings = {'stick:left', 'virtualStick:move'}})
input.defineAction({name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}})
ui.touchStick{action = 'move', mode = 'following', radius = 130, anchor = 'bottomLeft'}
ui.touchButton{action = 'jump', image = 'interface/icons/jump.svg', size = 170}
local x, y = input.vector('move')  -- The same for the touch stick, the keys and a gamepad.]]

TouchControls.speed = 520
TouchControls.radius = 70
TouchControls.sea = '#FF1E8DA6'
TouchControls.devices = {keyboardMouse = 'the keyboard and mouse', gamepad = 'a gamepad', touch = 'touch'}
TouchControls.directions = {'right', 'down and right', 'down', 'down and left', 'left', 'up and left', 'up', 'up and right'}
TouchControls.actions = {actions = {
    {name = 'move', type = 'vector', up = {'key:w'}, down = {'key:s'}, left = {'key:a'}, right = {'key:d'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'button:south', 'virtual:jump'}},
    {name = 'dash', type = 'button', bindings = {'key:leftShift', 'button:west', 'virtual:dash'}},
    {name = 'aim', type = 'vector', up = {'key:up'}, down = {'key:down'}, left = {'key:left'}, right = {'key:right'}, bindings = {'stick:right', 'virtualStick:aim'}},
}}
TouchControls.art = {
    stickBase = {image = 'interface/touch/stick-base.png', filter = 'linear'},
    stickKnob = {image = 'interface/touch/stick-knob.png', filter = 'linear'},
    touchButton = {image = 'interface/touch/touch-button.png', filter = 'linear'},
    touchButtonPressed = {image = 'interface/touch/touch-button-pressed.png', filter = 'linear'},
}

function TouchControls:init(entry)
    TouchControls.super.init(self, entry)
    self.ball = {height = 0, lift = 0, dash = 0, aimX = 1, aimY = 0, heading = -math.pi / 2, speed = 0}
    self.last = 'Nothing was pressed yet.'
    self.boat = assets.texture('interface/images/boat-top.png', {filter = 'linear'})
    -- Ripples drift on the sea at places a seed picks, so the water reads as water without an image.
    local random = m.random(5)
    self.ripples = {}
    for index = 1, 36 do
        self.ripples[index] = {x = random:nextFloat(), y = random:nextFloat(), size = random:range(14, 30), phase = random:range(0, math.pi * 2)}
    end
end

function TouchControls:enter()
    self:loadActions(TouchControls.actions)
    self:frame{
        hint = 'Move with the left stick, which follows a finger that leaves its ring, or with WASD or the left stick of a gamepad, and aim with the right stick, the arrows or the right stick of a gamepad. Jump with the round button, Space or the south button, and dash with the other button, Shift or the west button. Every control follows its own finger.',
        code = TouchControls.code,
        play = true,
        overlay = {
            ui.touchStick{id = 'stick', action = 'move', mode = 'following', radius = 130, width = 520, height = 320, anchor = 'bottomLeft', anchorTo = 'parent', margin = {0, 0, 64, 24}, style = {surfaces = TouchControls.art}},
            ui.row{anchor = 'bottomRight', anchorTo = 'parent', margin = {0, 44, 84, 0}, gap = 32, align = 'end',
                ui.touchStick{id = 'aim', action = 'aim', radius = 100, deadZone = 0.25},
                ui.column{gap = 24, style = {surfaces = TouchControls.art},
                    ui.touchButton{action = 'dash', image = 'interface/icons/dash.svg', size = 130, accessibilityLabel = 'Dash', onPress = function() self.last = 'Dash was pressed.' end},
                    ui.touchButton{action = 'jump', image = 'interface/icons/jump.svg', size = 170, accessibilityLabel = 'Jump', onPress = function() self.last = 'Jump was pressed.' end, onRelease = function() self.last = 'Jump was released.' end},
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
    ball.y = m.clamp(ball.y + y * speed * dt, radius, area.height - radius)
    ball.speed = math.min(1, math.sqrt(x * x + y * y))
    -- The bow turns smoothly toward the way the boat sails.
    if ball.speed > 0.1 then
        local target = math.atan(y, x)
        local turn = (target - ball.heading + math.pi) % (math.pi * 2) - math.pi
        ball.heading = ball.heading + turn * m.dampFactor(10, dt)
    end
    if input.pressed('jump') and ball.height == 0 then
        ball.lift = 900
    end
    ball.height = math.max(0, ball.height + ball.lift * dt)
    ball.lift = ball.height > 0 and ball.lift - 2600 * dt or 0
    local sector = math.floor(((math.atan(ball.aimY, ball.aimX) % (math.pi * 2)) / (math.pi / 4)) + 0.5) % 8 + 1
    self:status(string.format('The boat sails at %d percent of its speed and aims %s, with %s. %s', math.floor(ball.speed * 100 + 0.5), TouchControls.directions[sector], TouchControls.devices[input.lastDevice()] or 'an unknown device', self.last))
end

function TouchControls:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera)
    graphics2d.drawRect(graphics2d.canvasBounds(), TouchControls.sea, {layer = -1000})
    self:draw(self.area)
end

function TouchControls:draw(area)
    local time = haylen.elapsed()
    for _, ripple in ipairs(self.ripples) do
        local x = (ripple.x * area.width + time * 18) % (area.width + 80) - 40
        local y = ripple.y * area.height + math.sin(time + ripple.phase) * 6
        graphics2d.drawArc(x, y, ripple.size, 3, math.pi * 1.15, math.pi * 1.85, '#40FFFFFF', {layer = -10})
    end

    local ball, radius = self.ball, TouchControls.radius
    local lift = ball.height / 220
    local length = math.sqrt(ball.aimX * ball.aimX + ball.aimY * ball.aimY)
    local aimX, aimY = ball.x + ball.aimX / length * radius * 3, ball.y + ball.aimY / length * radius * 3
    graphics2d.drawRing(aimX, aimY, 18, 4, '#CCFFFFFF')
    graphics2d.drawCircle(aimX, aimY, 5, '#CCFFFFFF')
    if ball.speed > 0.1 or ball.dash > 0 then
        local backX, backY = ball.x - math.cos(ball.heading) * radius, ball.y - math.sin(ball.heading) * radius
        graphics2d.drawStroke({ball.x, ball.y, backX - math.cos(ball.heading) * 90, backY - math.sin(ball.heading) * 90}, {widths = {34, 4}, color = ball.dash > 0 and '#B0FFFFFF' or '#70FFFFFF', cap = 'round', layer = 1})
    end
    graphics2d.drawEllipse(ball.x + 10 + lift * 30, ball.y + 14 + lift * 40, radius * (0.9 - lift * 0.2), radius * (0.55 - lift * 0.1), '#40102030', {layer = 2})
    local scale = (radius * 2.4 / self.boat.height) * (1 + lift * 0.35)
    graphics2d.draw(self.boat, ball.x, ball.y - ball.height * 0.25, {rotation = ball.heading + math.pi / 2, scaleX = scale, scaleY = scale, layer = 3})
end

return TouchControls
