-- One-way platforms: a character jumps up through them and lands on them, drops down through them, and rides one that moves.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local OneWay = haylen.class('OneWay', PhysicsTest)

local kHero = 1
local kSolid = 2
local kOneWay = 4
local kSpeed = 360
local kJumpSpeed = 900
local kDropTime = 0.3

OneWay.actions = {
    {name = 'move', type = 'vector', up = {'key:w', 'key:up', 'button:dpadUp'}, down = {'key:s', 'key:down', 'button:dpadDown'}, left = {'key:a', 'key:left', 'button:dpadLeft'}, right = {'key:d', 'key:right', 'button:dpadRight'}, bindings = {'stick:left', 'virtualStick:move'}},
    {name = 'jump', type = 'button', bindings = {'key:space', 'key:w', 'key:up', 'button:south', 'virtual:jump'}},
}

function OneWay:enter()
    self:frame{
        hint = 'Move with A and D, the arrows or the left stick, jump with Space, W or the south button, and hold down while jumping to drop through a platform.',
        controls = {
            ui.checkbox{id = 'solid', text = 'Make the platforms solid', onChange = function(event) self:setSolid(event.checked) end},
            ui.button{id = 'reset', text = 'Start over', onClick = function() self:build() end},
        },
        play = true,
        pointer = false,
        actions = OneWay.actions,
        overlay = {
            ui.touchStick{action = 'move', radius = 110, floating = true, touchOnly = true, anchor = 'bottomLeft', margin = {0, 0, 110, 40}, width = 360, height = 300},
            ui.touchButton{action = 'jump', text = 'Jump', size = 150, touchOnly = true, anchor = 'bottomRight', margin = {0, 540, 110, 0}},
        },
    }
    self:build()
end

function OneWay:build()
    self.world = physics2d.newWorld({gravity = {0, 2200}})
    self.platforms, self.statics = {}, {}
    self.time, self.dropping = 0, 0

    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40, {category = kSolid})
    local left = self.world:createBody({type = 'static', x = -790, y = 0})
    parts.box(left, 20, 800, {category = kSolid})
    local right = self.world:createBody({type = 'static', x = 790, y = 0})
    parts.box(right, 20, 800, {category = kSolid})
    self.statics = {ground, left, right}

    for _, spot in ipairs({{-500, 250, 300}, {-100, 130, 280}, {300, 10, 300}, {-420, -110, 260}, {560, -220, 240}}) do
        self:platform('static', spot[1], spot[2], spot[3])
    end
    self.lift = self:platform('kinematic', 60, -250, 220)

    self.hero = self.world:createBody({x = -600, y = 330, fixedRotation = true, sleepEnabled = false})
    self.heroShape = parts.capsule(self.hero, 0, -22, 0, 22, 20, {friction = 0, category = kHero})
    parts.paint(self.hero, '#FF4DD0E1')
end

function OneWay:platform(kind, x, y, width)
    local body = self.world:createBody({type = kind, x = x, y = y})
    local shape = parts.box(body, width, 16, {category = kOneWay, oneWay = {0, -1}, friction = 0.8})
    parts.paint(body, kind == 'static' and '#FF8D6E63' or '#FFFFB74D')
    self.platforms[#self.platforms + 1] = {body = body, shape = shape}
    return body
end

function OneWay:setSolid(solid)
    for _, platform in ipairs(self.platforms) do
        platform.shape.oneWay = not solid and {0, -1} or nil
    end
end

-- The character stands when it is not rising and a short ray from its feet meets the ground or a platform.
function OneWay:grounded()
    if self.hero.velocity.y < -50 then
        return false
    end
    local x, y = self.hero.x, self.hero.y
    for _, offset in ipairs({-14, 0, 14}) do
        if self.world:raycast(x + offset, y + 40, x + offset, y + 48, {mask = kSolid | kOneWay}) then
            return true
        end
    end
    return false
end

function OneWay:exit()
    OneWay.super.exit(self)
    input.clearVirtual()
end

function OneWay:update(dt)
    OneWay.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    local moveX, moveY = input.vector('move')
    local velocity = self.hero.velocity
    local grounded = self:grounded()
    if input.pressed('jump') and grounded then
        if moveY > 0.5 then
            self.dropping = kDropTime
            self.heroShape.mask = kSolid
        else
            velocity.y = -kJumpSpeed
        end
    end
    self.hero.velocity = {moveX * kSpeed, velocity.y}

    self.dropping = math.max(0, self.dropping - dt)
    if self.dropping == 0 and self.heroShape.mask == kSolid then
        self.heroShape.mask = -1
    end
    self:status(string.format('%s, velocity %.0f, %.0f', grounded and 'Grounded' or 'In the air', velocity.x, velocity.y))
end

function OneWay:fixedUpdate(step)
    self.time = self.time + step
    self.lift.velocity = {math.cos(self.time * 0.8) * 220, 0}
    self:simulate(self.world, step)
end

function OneWay:draw(area)
    parts.drawAll(self.statics)
    for _, platform in ipairs(self.platforms) do
        parts.draw(platform.body)
        if platform.shape.oneWay then
            local body = platform.body
            for dx = -60, 60, 60 do
                graphics2d.drawPolygon({{body.x + dx - 8, body.y - 14}, {body.x + dx + 8, body.y - 14}, {body.x + dx, body.y - 26}}, '#88FFFFFF')
            end
        end
    end
    parts.draw(self.hero, {layer = 2})
end

return OneWay
