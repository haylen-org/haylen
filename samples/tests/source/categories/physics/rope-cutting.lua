-- A candy hanging from ropes made by `physics2d.newRope`, cut by swiping across them, where a ray cast along each move of the pointer finds the rope segments it crosses, so the candy swings and falls into a basket with a sensor.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local RopeCutting = haylen.class('RopeCutting', PhysicsTest)

local kRope = 2
local kCandy = {0, -60, 30}
local kPins = {{-300, -380}, {40, -400}, {330, -360}}
local kBasket = {-420, 340, 220, 110}
local kTrailLife = 0.25
local kRopeColors = {'#FFBCAAA4', '#FFA1887F', '#FFD7CCC8'}

function RopeCutting:enter()
    self:frame{
        hint = 'Swipe across a rope to cut it where you cross it, and drop the candy into the basket. R or the X button hangs a new candy.',
        controls = {
            ui.button{id = 'left', text = 'Cut the left rope', onClick = function() self:cutMiddle(1) end},
            ui.button{id = 'middle', text = 'Cut the middle rope', onClick = function() self:cutMiddle(2) end},
            ui.button{id = 'right', text = 'Cut the right rope', onClick = function() self:cutMiddle(3) end},
            ui.button{id = 'reset', text = 'Hang a new candy', onClick = function() self:build() end},
        },
        focus = 'left',
    }
    self.delivered = 0
    self:build()
end

function RopeCutting:build()
    self.world = physics2d.newWorld()
    self.trail, self.cuts, self.state = {}, 0, 'Hanging'

    local basket = self.world:createBody({type = 'static', x = kBasket[1], y = kBasket[2]})
    local width, height = kBasket[3], kBasket[4]
    parts.box(basket, width, 16, {offsetY = height / 2})
    parts.box(basket, 16, height, {offsetX = -width / 2})
    parts.box(basket, 16, height, {offsetX = width / 2})
    parts.paint(basket, '#FF8D6E63')
    basket:addBox(width - 40, height - 30, {sensor = true})
    local bumper = self.world:createBody({type = 'static', x = 150, y = 200})
    parts.circle(bumper, 40, {restitution = 0.6})
    self.statics = {basket, bumper}

    self.candy = self.world:createBody({x = kCandy[1], y = kCandy[2]})
    parts.circle(self.candy, kCandy[3], {restitution = 0.2})
    parts.paint(self.candy, '#FFF06292')

    -- Every segment knows its rope and its place in it, which the swipe reads from the bodies its ray hits.
    self.ropes = {}
    for index, pin in ipairs(kPins) do
        local rope = physics2d.newRope(self.world, {from = pin, to = {kCandy[1], kCandy[2]}, segments = 14, thickness = 6, density = 0.5, pinStart = true, endBody = self.candy, limitLength = false, category = kRope})
        local entry = {rope = rope, joints = rope:joints(), color = kRopeColors[index], cut = false}
        for place, body in ipairs(rope:bodies()) do
            body.data = {rope = entry, index = place}
        end
        self.ropes[index] = entry
    end

    self.world.onSensorBegin = function(sensor, visitor)
        if visitor == self.candy and self.state ~= 'Delivered' then
            self.state = 'Delivered'
            self.delivered = self.delivered + 1
        end
    end
end

-- Cutting at a segment destroys the joint that holds it to the next one, or the one before it for the last segment.
function RopeCutting:cut(entry, index)
    local segments = #entry.rope:bodies()
    local joint = entry.joints[math.min(index, segments - 1)]
    if joint.valid then
        joint:destroy()
        entry.cut = true
        self.cuts = self.cuts + 1
    end
end

function RopeCutting:cutMiddle(index)
    local entry = self.ropes[index]
    self:cut(entry, #entry.rope:bodies() // 2)
end

-- A held pointer cuts every rope segment that the segment from its last position to its new one crosses.
function RopeCutting:swipe(dt)
    local pointer = self.pointer
    local x, y = pointer.worldX, pointer.worldY
    if pointer.down and self.last and (x - self.last[1]) ^ 2 + (y - self.last[2]) ^ 2 > 1 then
        for _, hit in ipairs(self.world:raycastAll(self.last[1], self.last[2], x, y, {mask = kRope})) do
            local data = hit.body.data
            self:cut(data.rope, data.index)
        end
    end
    self.last = pointer.down and {x, y} or nil
    if pointer.down then
        self.trail[#self.trail + 1] = {x = x, y = y, life = kTrailLife, start = pointer.pressed}
    end
    for index = #self.trail, 1, -1 do
        local point = self.trail[index]
        point.life = point.life - dt
        if point.life <= 0 then
            table.remove(self.trail, index)
        end
    end
end

function RopeCutting:update(dt)
    RopeCutting.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self:swipe(dt)
    if self.state == 'Hanging' and self.candy.y > 470 then
        self.state = 'Missed'
    end
    local holding = 0
    for _, entry in ipairs(self.ropes) do
        holding = holding + (entry.cut and 0 or 1)
    end
    local velocity = self.candy.velocity
    self:status(string.format('%s, ropes holding %d, cuts %d, candy speed %.0f, delivered %d, step %.2f ms', self.state, holding, self.cuts, math.sqrt(velocity.x ^ 2 + velocity.y ^ 2), self.delivered, self:stepTime()))
end

function RopeCutting:fixedUpdate(step)
    self:simulate(self.world, step)
end

function RopeCutting:draw(area)
    graphics2d.drawRect({kBasket[1] - kBasket[3] / 2 + 20, kBasket[2] - kBasket[4] / 2 + 15, kBasket[3] - 40, kBasket[4] - 30}, self.state == 'Delivered' and '#5566BB6A' or '#2266BB6A', {layer = -1})
    parts.drawAll(self.statics)
    for index, entry in ipairs(self.ropes) do
        for _, segment in ipairs(entry.rope:segments()) do
            local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
            local hx, hy = cos * segment.length / 2, sin * segment.length / 2
            graphics2d.drawLine(segment.x - hx, segment.y - hy, segment.x + hx, segment.y + hy, 6, entry.color, {layer = 1})
        end
        graphics2d.drawCircle(kPins[index][1], kPins[index][2], 10, '#FF90A4AE', {layer = 2})
    end
    local candy = self.candy
    local cos, sin = math.cos(candy.rotation), math.sin(candy.rotation)
    for _, side in ipairs({-1, 1}) do
        local x, y = candy.x + cos * side * 28, candy.y + sin * side * 28
        graphics2d.drawPolygon({{x, y}, {x + (cos * 24 - sin * 18) * side, y + (sin * 24 + cos * 18) * side}, {x + (cos * 24 + sin * 18) * side, y + (sin * 24 - cos * 18) * side}}, '#FFF8BBD0', {layer = 2})
    end
    parts.draw(candy, {layer = 3})
    for index = 2, #self.trail do
        local a, b = self.trail[index - 1], self.trail[index]
        if not b.start then
            graphics2d.drawLine(a.x, a.y, b.x, b.y, 8 * b.life / kTrailLife + 1, m.color(1, 1, 1, b.life / kTrailLife), {layer = 4})
        end
    end
end

return RopeCutting
