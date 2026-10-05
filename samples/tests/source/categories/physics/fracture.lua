-- Objects that `physics2d.fracture` breaks into Voronoi pieces around the point they are hit, by a tap or a click or by a fast wrecking ball.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Fracture = haylen.class('Fracture', PhysicsTest)

local kBreakSpeed = 500

function Fracture:enter()
    self:frame{
        hint = 'Tap or click an object to break it where you hit it, or swing the wrecking ball. Pieces draw with the debug outlines of the world. R or the X button rebuilds.',
        controls = {
            ui.label{text = 'Pieces', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'pieces', value = 10, min = 2, max = 30, step = 1, showValue = true, decimals = 0, onChange = function(event) self.pieces = event.value end},
            ui.button{id = 'swing', text = 'Swing the wrecking ball', onClick = function() self:swing() end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'swing',
    }
    self.pieces = 10
    self.seed = 1
    self:build()
end

function Fracture:build()
    self.world = physics2d.newWorld()
    self.whole, self.fragments = {}, 0
    local ground = self.world:createBody({type = 'static', x = 0, y = 410})
    parts.box(ground, 1600, 40)
    self.statics = {ground}

    self:object(-150, 270, function(body) parts.box(body, 30, 240) end, '#AA90CAF9')
    self:object(50, 270, function(body) parts.box(body, 30, 240) end, '#AA90CAF9')
    self:object(-50, 132, function(body) parts.box(body, 300, 36) end, '#FFBCAAA4')
    self:object(300, 300, function(body) parts.polygon(body, {{-90, 80}, {-110, -10}, {-40, -80}, {60, -70}, {110, 20}, {70, 80}}) end, '#FF9E9E9E')
    for index = 0, 2 do
        self:object(-520 + index * 90, 350, function(body) parts.box(body, 80, 80) end, '#FFFFB74D')
    end
    self:object(-475, 270, function(body) parts.box(body, 80, 80) end, '#FFFFB74D')

    local pivot = self.world:createBody({type = 'static', x = 560, y = -420})
    self.ball = self.world:createBody({x = 560, y = 260, bullet = true})
    parts.circle(self.ball, 50, {density = 20})
    parts.paint(self.ball, '#FF546E7A')
    self.world:createJoint('distance', pivot, self.ball, {ax = 560, ay = -420, bx = 560, by = 260})
    self.pivot = pivot

    self.world.onHit = function(a, b, contact)
        if contact.speed > kBreakSpeed then
            for _, body in ipairs({a, b}) do
                if body.valid and body.data and body.data.breakable then
                    self:shatter(body, contact.x, contact.y)
                end
            end
        end
    end
end

function Fracture:object(x, y, build, color)
    local body = self.world:createBody({x = x, y = y})
    build(body)
    parts.paint(body, color)
    body.data.breakable = true
    self.whole[#self.whole + 1] = body
end

function Fracture:shatter(body, x, y)
    self.seed = self.seed + 1
    local pieces = physics2d.fracture(body, {pieces = self.pieces, impact = {x, y}, seed = self.seed})
    self.fragments = self.fragments + #pieces
end

function Fracture:swing()
    self.ball:applyImpulse(-self.ball.mass * 1400, 0)
end

function Fracture:update(dt)
    Fracture.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        for _, shape in ipairs(self.world:pick(self.camera, self.pointer.x, self.pointer.y)) do
            if shape.body.data and shape.body.data.breakable then
                self:shatter(shape.body, self.pointer.worldX, self.pointer.worldY)
                break
            end
        end
    end
    local intact = 0
    for _, body in ipairs(self.whole) do
        intact = intact + (body.valid and 1 or 0)
    end
    self:status(string.format('Whole objects %d, pieces %d, bodies %d, step %.2f ms', intact, self.fragments, self.world.bodyCount, self:stepTime()))
end

function Fracture:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Fracture:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.whole)
    parts.draw(self.ball, {layer = 1})
    graphics2d.drawLine(self.pivot.x, self.pivot.y, self.ball.x, self.ball.y, 4, '#FF90A4AE')
    self.world:debugDraw({layer = 2})
end

return Fracture
