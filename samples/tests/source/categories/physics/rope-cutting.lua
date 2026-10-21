-- A wrapped candy hanging from braided ropes made by `physics2d.newRope`, cut by swiping across them, where a ray cast along each move of the pointer finds the rope segments it crosses, so the candy swings and falls into a wicker basket with a sensor. The right rope snaps by itself when the test opens.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local RopeCutting = haylen.class('RopeCutting', PhysicsTest)

RopeCutting.code = [[
local rope = physics2d.newRope(world, {from = {-300, -380}, to = {0, -60}, segments = 14, thickness = 6, density = 0.5, pinStart = true, endBody = candy, limitLength = false, category = kRope})
for _, hit in ipairs(world:raycastAll(lastX, lastY, x, y, {mask = kRope})) do cut(hit.body) end  -- The segments the swipe crossed.
rope:joints()[index]:destroy()  -- Cuts the rope after that segment.
basket:addBox(180, 80, {sensor = true})]]

RopeCutting.ropeCategory = 2
RopeCutting.candySpot = {0, -60, 30}
RopeCutting.pins = {{-300, -380}, {40, -400}, {330, -360}}
RopeCutting.basket = {-420, 340, 220, 110}
RopeCutting.bumper = {150, 200, 40}
RopeCutting.ground = 400
RopeCutting.trailLife = 0.25
RopeCutting.ropeWidth = 9
RopeCutting.firstCut = 1.2
-- The candy picture is wider than the ball of the body by its wrappers.
RopeCutting.candySize = {134, 78}

function RopeCutting:enter()
    self:frame{
        code = RopeCutting.code,
        hint = 'Swipe across a rope to cut it where you cross it, and drop the candy into the basket. R or the X button hangs a new candy.',
        controls = {
            ui.button{id = 'left', text = 'Cut the left rope', onClick = function() self:cutMiddle(1) end},
            ui.button{id = 'middle', text = 'Cut the middle rope', onClick = function() self:cutMiddle(2) end},
            ui.button{id = 'right', text = 'Cut the right rope', onClick = function() self:cutMiddle(3) end},
            ui.button{id = 'reset', text = 'Hang a new candy', onClick = function() self:build() end},
        },
        focus = 'left',
        view = {1700, 840},
    }
    self.camera.position = {0, -10}
    self.delivered = 0
    self.candyPicture = assets.texture('physics/candy.png', {filter = 'linear'})
    self.cheers = Burst('celebration/confetti-pop', {count = 3, scale = 0.8, layer = 30})
    self.piece = {}
    self.trailStroke = {width = 1, color = '#FFFFFFFF', cap = 'round', layer = 40}
    self.trailLine = {0, 0, 0, 0}
    self:build()
    timer.after(RopeCutting.firstCut, function() self:cutMiddle(3) end, {owner = self})
end

function RopeCutting:build()
    self.world = physics2d.newWorld()
    self.trail, self.cuts, self.state = {}, 0, 'hanging'

    local spot = RopeCutting.basket
    local basket = self.world:createBody({type = 'static', x = spot[1], y = spot[2]})
    local width, height = spot[3], spot[4]
    basket:addBox(width, 16, {offsetY = height / 2})
    basket:addBox(16, height, {offsetX = -width / 2})
    basket:addBox(16, height, {offsetX = width / 2})
    basket:addBox(width - 40, height - 30, {sensor = true})
    local ground = self.world:createBody({type = 'static', y = RopeCutting.ground + 20})
    ground:addBox(4000, 40)
    local bumper = RopeCutting.bumper
    local post = self.world:createBody({type = 'static', x = bumper[1], y = bumper[2]})
    post:addCircle(bumper[3], {restitution = 0.6})

    self.candy = self.world:createBody({x = RopeCutting.candySpot[1], y = RopeCutting.candySpot[2]})
    self.candy:addCircle(RopeCutting.candySpot[3], {restitution = 0.2})

    -- Every segment knows its rope and its place in it, which the swipe reads from the bodies its ray hits.
    self.ropes = {}
    for index, pin in ipairs(RopeCutting.pins) do
        local rope = physics2d.newRope(self.world, {from = pin, to = {self.candy.x, self.candy.y}, segments = 14, thickness = 6, density = 0.5, pinStart = true, endBody = self.candy, limitLength = false, category = RopeCutting.ropeCategory})
        local entry = {rope = rope, joints = rope:joints(), cut = false}
        for place, body in ipairs(rope:bodies()) do
            body.data = {rope = entry, index = place}
        end
        self.ropes[index] = entry
    end

    self.world.onSensorBegin = function(sensor, visitor)
        if visitor == self.candy and self.state ~= 'delivered' then
            self.state = 'delivered'
            self.delivered = self.delivered + 1
            self.cheers:fire(spot[1], spot[2] - 40)
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
    if pointer.pressed and ui.usingPointer() then
        self.blocked = true
    elseif not pointer.down then
        self.blocked = false
    end
    local swiping = pointer.down and not self.blocked
    if swiping and self.lastX and (x - self.lastX) ^ 2 + (y - self.lastY) ^ 2 > 1 then
        for _, hit in ipairs(self.world:raycastAll(self.lastX, self.lastY, x, y, {mask = RopeCutting.ropeCategory})) do
            local data = hit.body.data
            self:cut(data.rope, data.index)
        end
    end
    if swiping then
        self.lastX, self.lastY = x, y
        self.trail[#self.trail + 1] = {x = x, y = y, life = RopeCutting.trailLife, start = pointer.pressed}
    else
        self.lastX, self.lastY = nil, nil
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
    self.cheers:update(dt)
    if self.state == 'hanging' and self.candy.y > RopeCutting.ground - 40 then
        self.state = 'missed'
    end
    local holding = 0
    for _, entry in ipairs(self.ropes) do
        holding = holding + (entry.cut and 0 or 1)
    end
    local candy = holding == 0 and 'falls free' or string.format('hangs from %d %s', holding, holding == 1 and 'rope' or 'ropes')
    if self.state == 'delivered' then
        candy = 'landed in the basket'
    elseif self.state == 'missed' then
        candy = 'missed the basket'
    end
    self:status(string.format('The candy %s after %d %s, and %d %s reached the basket so far.', candy, self.cuts, self.cuts == 1 and 'cut' or 'cuts', self.delivered, self.delivered == 1 and 'candy has' or 'candies have'))
end

function RopeCutting:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- A rope draws as a braid through the ends of its segments, in pieces where it was cut.
function RopeCutting:drawRope(entry)
    local segments = entry.rope:segments()
    local piece = self.piece
    local count = 0
    for index, segment in ipairs(segments) do
        local cos, sin = math.cos(segment.rotation), math.sin(segment.rotation)
        local hx, hy = cos * segment.length / 2, sin * segment.length / 2
        if index > 1 and not entry.joints[index - 1].valid then
            self:flushRope(count)
            count = 0
        end
        if count == 0 then
            piece[1], piece[2] = segment.x - hx, segment.y - hy
            count = 2
        end
        piece[count + 1], piece[count + 2] = segment.x + hx, segment.y + hy
        count = count + 2
    end
    self:flushRope(count)
end

function RopeCutting:flushRope(count)
    local piece = self.piece
    for index = #piece, count + 1, -1 do
        piece[index] = nil
    end
    if count >= 4 then
        Look.rope(piece, RopeCutting.ropeWidth, 1)
    end
end

-- The basket is woven wicker seen from the side, with bands of reeds between upright stakes under a rolled rim, in front of what falls into it.
function RopeCutting:drawBasket()
    local unit = Look.pixel
    local spot = RopeCutting.basket
    local x, y, width, height = spot[1], spot[2], spot[3], spot[4]
    local left, top = x - width / 2 - 8, y - height / 2
    graphics2d.drawShape({left, top, width + 16, height + 8}, {radius = {4, 4, 26, 26}, color = '#FFB8743A', borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = 4})
    for band = 0, 4 do
        local by = top + 12 + band * (height - 12) / 5
        graphics2d.drawShape({left + 6 + (band % 2) * 6, by, width + 4 - (band % 2) * 12, (height - 12) / 5 - 4}, {radius = 7, color = band % 2 == 0 and '#FFE9A65B' or '#FFD18A45', layer = 4})
    end
    for stake = 1, 7 do
        local sx = left + stake * (width + 16) / 8
        graphics2d.drawLine(sx, top + 10, sx, top + height, 3 * unit, '#998C5A32', {layer = 4})
    end
    Look.block(x, top, width + 28, 16, Look.tint('wood', '#FFD18A45', '#FFA86B3C', '#FFF2C88E'), 4)
end

function RopeCutting:draw(area)
    local bounds = graphics2d.canvasBounds()
    Scenery.sky(self.camera, RopeCutting.ground)
    Scenery.ground(RopeCutting.ground)
    self:drawBasket()
    local bumper = RopeCutting.bumper
    Look.block(bumper[1], (bumper[2] + RopeCutting.ground) / 2 + 20, 18, RopeCutting.ground - bumper[2] - 40, 'darkWood', 0)
    Look.disc(bumper[1], bumper[2], bumper[3], 'red', 0)
    graphics2d.drawRing(bumper[1], bumper[2], bumper[3] * 0.6, 6, '#FFFBF6EA', {layer = 0})

    for index, entry in ipairs(self.ropes) do
        local pin = RopeCutting.pins[index]
        Look.line(pin[1], bounds.y, pin[1], pin[2], 6, '#FF9AA5BE', 1)
        self:drawRope(entry)
        Look.pin(pin[1], pin[2], 11, 2)
    end
    local candy, size = self.candy, RopeCutting.candySize
    graphics2d.draw(self.candyPicture, candy.x, candy.y, {width = size[1], height = size[2], rotation = candy.rotation, layer = 3})
    self.cheers:draw()

    local line, stroke = self.trailLine, self.trailStroke
    for index = 2, #self.trail do
        local a, b = self.trail[index - 1], self.trail[index]
        if not b.start then
            local share = b.life / RopeCutting.trailLife
            line[1], line[2], line[3], line[4] = a.x, a.y, b.x, b.y
            stroke.width, stroke.color = (10 * share + 2) * Look.pixel, {1, 1, 1, share}
            graphics2d.drawStroke(line, stroke)
        end
    end
end

return RopeCutting
