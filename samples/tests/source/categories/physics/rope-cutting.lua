-- A wrapped candy hanging from braided ropes made by `physics2d.newRope` under the beam of a wooden frame, cut by swiping across them, where a ray cast along each move of the pointer finds the rope segments it crosses, so the candy swings and falls into a wicker basket with a sensor. The right rope snaps by itself when the test opens.
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
local rope = physics2d.newRope(world, {from = {-300, -380}, to = {0, -60}, segments = 18, thickness = 6,
    angularDamping = 2, pinStart = true, endBody = candy, limitLength = false, category = kRope})
for _, hit in ipairs(world:raycastAll(lastX, lastY, x, y, {mask = kRope})) do
    rope:joints()[hit.body.data.index]:destroy()  -- Cuts the rope after each segment the swipe crossed.
end
basket:addBox(180, 80, {sensor = true})]]

RopeCutting.ropeCategory = 2
RopeCutting.candySpot = {0, -60, 30}
RopeCutting.pins = {{-300, -380}, {40, -380}, {330, -380}}
-- The frame is a beam on two posts, by the middle of the beam, the half width between the posts and the thickness of its timbers.
RopeCutting.timbers = {beam = -405, half = 640, timber = 26}
RopeCutting.segments = 18
-- Each stretch of a rope between two joints bends through this many points, so a loose end hangs in a curve.
RopeCutting.smoothness = 4
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
        view = {1700, 880},
    }
    self.camera.position = {0, -10}
    self.delivered = 0
    self.candyPicture = assets.texture('physics/candy.png', {filter = 'linear'})
    self.cheers = Burst('celebration/confetti-pop', {count = 3, scale = 0.8, layer = 30})
    self.piece, self.curve = {}, {}
    self.wicker, self.wickerFlat, self.rim = {}, {}, {0, 0, 0, 0}
    self.wickerStroke = {width = 1, color = Look.ink, join = 'round', closed = true, layer = 4}
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
        local rope = physics2d.newRope(self.world, {from = pin, to = {self.candy.x, self.candy.y}, segments = RopeCutting.segments, thickness = 6, density = 0.5, angularDamping = 2, pinStart = true, endBody = self.candy, limitLength = false, category = RopeCutting.ropeCategory})
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

-- Draws a piece of a rope as a smooth Catmull-Rom curve through the ends of its segments, so a loose end that swings hangs in a curve and never folds in a sharp corner.
function RopeCutting:flushRope(count)
    if count < 4 then
        return
    end
    local piece, curve = self.piece, self.curve
    local points, steps = count // 2, RopeCutting.smoothness
    local size = 0
    for index = 1, points - 1 do
        local i0, i1, i2, i3 = math.max(index - 1, 1), index, index + 1, math.min(index + 2, points)
        for step = 0, steps - 1 do
            local t = step / steps
            local t2, t3 = t * t, t * t * t
            for axis = 1, 2 do
                local p0, p1, p2, p3 = piece[i0 * 2 - 2 + axis], piece[i1 * 2 - 2 + axis], piece[i2 * 2 - 2 + axis], piece[i3 * 2 - 2 + axis]
                curve[size + axis] = 0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (3 * p1 - p0 - 3 * p2 + p3) * t3)
            end
            size = size + 2
        end
    end
    curve[size + 1], curve[size + 2] = piece[count - 1], piece[count]
    size = size + 2
    for index = #curve, size + 1, -1 do
        curve[index] = nil
    end
    Look.rope(curve, RopeCutting.ropeWidth, 1)
end

-- The frame is a wooden beam on two posts with braces in its corners, and every rope hangs from a steel eye under the beam.
function RopeCutting:drawFrame()
    local frame = RopeCutting.timbers
    local timber, beam = frame.timber, frame.beam
    local postTop = beam - timber / 2
    for _, side in ipairs({-1, 1}) do
        local x = side * frame.half
        Look.block(x, (postTop + RopeCutting.ground) / 2, timber, RopeCutting.ground - postTop, 'darkWood', 0)
        Look.block(x - side * 46, beam + 46, 16, 108, 'darkWood', 0, side * math.pi / 4)
    end
    Look.block(0, beam, frame.half * 2 + timber * 2, timber, 'darkWood', 1)
    for _, pin in ipairs(RopeCutting.pins) do
        Look.line(pin[1], beam + timber / 2 - 2, pin[1], pin[2], 6, '#FF9AA5BE', 1)
    end
end

-- The basket is wicker seen from the side, wider at its mouth: rows of reeds woven over and under the stakes, each row shifted by half a stake, under a braided rim, in front of what falls into it.
function RopeCutting:drawBasket()
    local unit = Look.pixel
    local spot = RopeCutting.basket
    local x, y, width, height = spot[1], spot[2], spot[3], spot[4]
    local top, bottom = y - height / 2, y + height / 2 + 8
    local mouth, base = width / 2 + 12, width / 2 - 8
    local outline = self.wicker
    outline[1], outline[2], outline[3], outline[4] = {x - mouth, top}, {x + mouth, top}, {x + base, bottom - 12}, {x + base - 14, bottom}
    outline[5], outline[6] = {x - base + 14, bottom}, {x - base, bottom - 12}
    graphics2d.drawPolygon(outline, '#FF7A4724', {layer = 4})

    local rows, stakes = 5, 7
    local rowHeight = (bottom - top - 16) / rows
    for row = 0, rows - 1 do
        local rowTop = top + 12 + row * rowHeight
        local share = (rowTop + rowHeight / 2 - top) / (bottom - top)
        local half = mouth + (base - mouth) * share - 5
        local length = half * 2 / stakes
        local shift = row % 2 == 0 and 0 or length / 2
        for stake = -1, stakes do
            local left = math.max(x - half, x - half + stake * length + shift)
            local right = math.min(x + half, x - half + (stake + 1) * length + shift - 3)
            if right - left > 6 then
                graphics2d.drawShape({left, rowTop, right - left, rowHeight - 3}, {radius = (rowHeight - 3) / 2, color = row % 2 == 0 and '#FFE0A35A' or '#FFD39450', layer = 4})
                graphics2d.drawShape({left + 3, rowTop + 2, right - left - 6, (rowHeight - 3) * 0.35}, {radius = rowHeight * 0.2, color = '#B3F7D49A', layer = 4})
            end
        end
    end

    local flat = self.wickerFlat
    for index, point in ipairs(outline) do
        flat[index * 2 - 1], flat[index * 2] = point[1], point[2]
    end
    self.wickerStroke.width = Look.outlinePixels * unit
    graphics2d.drawStroke(flat, self.wickerStroke)
    local rim = self.rim
    rim[1], rim[2], rim[3], rim[4] = x - mouth - 8, top, x + mouth + 8, top
    Look.rope(rim, 20, 4)
end

function RopeCutting:draw(area)
    Scenery.sky(self.camera, RopeCutting.ground)
    Scenery.ground(RopeCutting.ground)
    self:drawFrame()
    self:drawBasket()
    local bumper = RopeCutting.bumper
    Look.block(bumper[1], (bumper[2] + RopeCutting.ground) / 2 + 20, 18, RopeCutting.ground - bumper[2] - 40, 'darkWood', 0)
    Look.disc(bumper[1], bumper[2], bumper[3], 'red', 0)
    graphics2d.drawRing(bumper[1], bumper[2], bumper[3] * 0.6, 6, '#FFFBF6EA', {layer = 0})

    for index, entry in ipairs(self.ropes) do
        local pin = RopeCutting.pins[index]
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
