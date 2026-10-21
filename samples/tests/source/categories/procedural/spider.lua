-- Spider: a spider seen from the side that climbs a generated cave over its floor, walls, ceiling and floating rocks. Its body is a cephalothorax with the eyes, the fangs and the palps in front of a raised abdomen on a thin waist, and its eight legs grow from the sides of the cephalothorax as chains of three segments, a femur that rises to a high knee, a tibia and a tarsus that comes down to the foot. The body walks the surface at a fixed height and takes the shorter way around its rock to a point, the feet step in the alternating wave of a spider as it travels, and a point on another rock makes it leap across on a silk thread.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Cave = require('categories.procedural.cave')
local Crawler = require('categories.procedural.crawler')
local Leg = require('categories.procedural.leg')

local Spider = haylen.class('Spider', AlgorithmTest)

Spider.code = [[
local leg = animation2d.newChain({x = hipX, y = hipY, lengths = {60, 66}})  -- Femur and tibia, down to the ankle over the foot.
leg:reach(ankleX, ankleY, {pole = {hipX + normalX * 100 + tangentX * 60, hipY + normalY * 100 + tangentY * 60}})  -- High knees that lean out.
crawler:setTarget(x, y)  -- Traces the outline of the rock and takes the shorter way around.
Leg.wave(legs, crawler.travelled, stride, 0.45, crawler.velocity, land)  -- Each leg steps at its phase of the stride.]]

Spider.height = 40
Spider.stride = 54
Spider.swing = 0.45
Spider.lengths = {60, 66}
Spider.tarsus = 34
Spider.view = {1240, 580}
-- Each leg: where its hip sits along the body and over its middle, where its foot rests along the body, the side it is on and its phase of the gait, so the first and third legs of one side step with the second and fourth of the other.
Spider.legs = {
    {hip = 24, lift = -8, foot = 128, side = 'near', phase = 0}, {hip = 17, lift = -10, foot = 70, side = 'near', phase = 0.5},
    {hip = 9, lift = -10, foot = -36, side = 'near', phase = 0.06}, {hip = 2, lift = -8, foot = -96, side = 'near', phase = 0.56},
    {hip = 27, lift = -2, foot = 138, side = 'far', phase = 0.5}, {hip = 20, lift = -4, foot = 84, side = 'far', phase = 0},
    {hip = 12, lift = -4, foot = -22, side = 'far', phase = 0.56}, {hip = 5, lift = -2, foot = -84, side = 'far', phase = 0.06},
}
Spider.colors = {near = '#FF9A6440', far = '#FF4A2E1F', joint = '#FFB7804F', body = '#FF8A5636', light = '#FFB5774A', abdomen = '#FF6B4029', band = '#FFE0A93C', outline = '#FF1E1B33', eye = '#FF15101A', shine = '#FFFFFFFF', fang = '#FF2E1C12', silk = '#FFE8EAF2'}

function Spider:enter()
    self.seed = 3
    self.speed = 200
    self.showGuides = false
    self:build()
    self:frame{
        view = Spider.view,
        code = Spider.code,
        hint = 'Click or tap a point and the spider climbs the shorter way around its rock to the nearest spot, or leaps across on a thread when the point is on another rock. Without a point it wanders. R or the X button makes a new cave.',
        controls = {
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 40, max = 420, step = 10, value = self.speed, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end}},
            ui.checkbox{id = 'guides', text = 'Show the footholds and the knees', checked = false, onChange = function(event) self.showGuides = event.checked end},
            ui.button{id = 'wander', text = 'Let it wander', onClick = function() self:setTarget(nil) end},
            ui.button{id = 'cave', text = 'New cave', variant = 'primary', onClick = function() self:newCave() end},
        },
    }
end

function Spider:newCave()
    self.seed = self.seed + 1
    self:build()
end

function Spider:build()
    -- A foothold lies on the rock under the body, walked along it from the rest point in the way the spider goes.
    self.land = function(leg, lead)
        return self.cave:walk(leg.restX, leg.restY, lead * self.crawler.dir, 0)
    end
    self.cave = Cave(self.seed, 1200, 540)
    self.crawler = Crawler(self.cave, 0, 400, Spider.height, self.speed, self.seed)
    self.facing = 1
    self.legs = {}
    for _, layout in ipairs(Spider.legs) do
        local leg = Leg({x = self.crawler.x, y = self.crawler.y, lengths = Spider.lengths, tarsus = Spider.tarsus, phase = layout.phase, lift = 22, widths = {9, 7, 5, 3}})
        leg.layout = layout
        self.legs[#self.legs + 1] = leg
    end
    self.palps = {}
    for _, side in ipairs({'far', 'near'}) do
        local palp = Leg({x = self.crawler.x, y = self.crawler.y, lengths = {16, 18}, widths = {7, 6, 4}})
        palp.side = side
        self.palps[#self.palps + 1] = palp
    end
    self:placeLegs(true)
    self.target, self.leap, self.thread = nil, nil, nil
    self.leaps = 0
    self.leapTime = 6
end

function Spider:setTarget(x, y)
    self.target = x and {x, y} or nil
    self.crawler:setTarget(x, y)
end

-- Returns the point at `along` the body ahead of its middle, mirrored by the way it faces, and `lift` away from the rock.
function Spider:at(along, lift)
    local crawler = self.crawler
    local nx, ny = crawler.nx, crawler.ny
    local a = along * self.facing
    return crawler.x - ny * a + nx * lift, crawler.y + nx * a + ny * lift
end

-- Sets the hip, the rest point and the pole of every leg from the body, and puts the feet down at their rest points when `settle` is set.
function Spider:placeLegs(settle)
    local groundX, groundY = self:at(0, -Spider.height)
    for _, leg in ipairs(self.legs) do
        local layout = leg.layout
        leg.hipX, leg.hipY = self:at(layout.hip, layout.lift)
        leg.restX, leg.restY = self.cave:walk(groundX, groundY, layout.foot * self.facing, 0)
        -- The knee rises over the hip and leans the way the leg reaches.
        local outward = layout.foot >= layout.hip and 60 or -60
        leg.poleX, leg.poleY = self:at(layout.hip + outward, layout.lift + 100)
        if settle then
            leg:place(leg.restX, leg.restY)
        end
    end
    -- The palps hang in front of the fangs and feel the way ahead.
    local time = haylen.elapsed()
    for index, palp in ipairs(self.palps) do
        palp.hipX, palp.hipY = self:at(36, -4)
        palp:place(self:at(56 + index * 4, -22 + math.sin(time * 6 + index * 2) * 4))
        palp.poleX, palp.poleY = self:at(46, 30)
    end
end

-- Starts a leap to the surface under a point when the straight line there stays clear of the rocks.
function Spider:tryLeap(x, y)
    local cave, crawler = self.cave, self.crawler
    local goalX, goalY = cave:project(x, y, Spider.height)
    local distance = math.sqrt((goalX - crawler.x) ^ 2 + (goalY - crawler.y) ^ 2)
    if distance < 120 or not cave:clear(crawler.x, crawler.y, goalX, goalY, Spider.height * 0.6) then
        return false
    end
    crawler.arrived = false
    self.leap = {fromX = crawler.x, fromY = crawler.y, toX = goalX, toY = goalY, time = 0, duration = m.clamp(distance / 700, 0.3, 1.2), distance = distance}
    local anchorX, anchorY = self:at(-84, -Spider.height)
    self.thread = {x = anchorX, y = anchorY, life = 1}
    self.leaps = self.leaps + 1
    self:log('Leap %d of %.0f units on a thread.', self.leaps, distance)
    return true
end

function Spider:updateLeap(dt)
    local leap, crawler = self.leap, self.crawler
    leap.time = leap.time + dt
    local t = math.min(1, leap.time / leap.duration)
    crawler.x = leap.fromX + (leap.toX - leap.fromX) * t
    crawler.y = leap.fromY + (leap.toY - leap.fromY) * t - math.sin(t * math.pi) * leap.distance * 0.12
    if t >= 1 then
        crawler:land(leap.toX, leap.toY)
        self.leap = nil
        if self.target then
            self.crawler:setTarget(self.target[1], self.target[2])
        end
        return
    end
    -- The legs fold under the body while it flies.
    self:placeLegs(false)
    for _, leg in ipairs(self.legs) do
        leg:place(self:at(leg.layout.hip + (leg.layout.foot - leg.layout.hip) * 0.35, -Spider.height * 0.7))
    end
end

function Spider:update(dt)
    Spider.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:newCave()
    end
    local pointer = self.pointer
    if pointer.pressed and not self.leap then
        self:setTarget(pointer.worldX, pointer.worldY)
    end

    local crawler = self.crawler
    crawler.speed = self.speed
    if self.leap then
        self:updateLeap(dt)
    else
        crawler:update(dt)
        if self.target and crawler.arrived and crawler.unreachable then
            self:tryLeap(self.target[1], self.target[2])
        elseif not self.target then
            self.leapTime = self.leapTime - dt
            if self.leapTime <= 0 then
                self.leapTime = crawler.random:range(5, 9)
                self:tryLeap(crawler.random:range(-self.cave.halfWidth, self.cave.halfWidth), crawler.random:range(-self.cave.halfHeight, self.cave.halfHeight))
            end
        end
    end
    if self.thread then
        self.thread.life = self.thread.life - (self.leap and 0 or dt / 1.5)
        self.thread = self.thread.life > 0 and self.thread or nil
    end

    -- The body turns to face the way it walks while it stands still.
    self.facing = self.facing + (crawler.dir - self.facing) * m.dampFactor(12, dt)
    self:placeLegs(false)
    if not self.leap then
        Leg.wave(self.legs, crawler.travelled, Spider.stride, Spider.swing, crawler.velocity, self.land)
    end
    local stepping = 0
    for _, leg in ipairs(self.legs) do
        leg:update(dt, leg.hipX, leg.hipY, crawler.nx, crawler.ny, leg.poleX, leg.poleY)
        stepping = stepping + (leg:stepping() and 1 or 0)
    end
    for _, palp in ipairs(self.palps) do
        palp:update(dt, palp.hipX, palp.hipY, crawler.nx, crawler.ny, palp.poleX, palp.poleY)
    end
    local state = 'wandering'
    if self.leap then
        state = 'leaping'
    elseif self.target then
        state = crawler.arrived and (crawler.unreachable and 'at the nearest spot, with no clear leap' or 'at the point') or 'climbing to the point'
    end
    local leaps = self.leaps == 1 and 'once' or self.leaps .. ' times'
    self:status(string.format('In cave %d, the spider is %s. It has %d of its 8 legs in a step and leapt %s.', self.seed, state, stepping, leaps))
end

-- Draws an ellipse centered `along` the body and `lift` over its middle, tilted by `tilt` toward the back of the body, and narrower while the body turns, with the outline of the art style unless it is a `plain` patch of light on a part.
function Spider:ellipse(along, lift, width, height, tilt, color, layer, plain)
    local crawler = self.crawler
    local x, y = self:at(along, lift)
    local dx, dy = math.cos(tilt) * (self.facing >= 0 and 1 or -1), math.sin(tilt)
    local angle = math.atan(crawler.nx * dx + crawler.ny * dy, -crawler.ny * dx + crawler.nx * dy)
    local across = math.max(0.35, math.abs(self.facing))
    local wide = width * across
    graphics2d.drawShape({x - wide / 2, y - height / 2, wide, height}, {kind = 'ellipse', rotation = angle, color = color, borderWidth = plain and 0 or 3.5, borderColor = Spider.colors.outline, layer = layer})
end

-- The body draws from the back to the front: the abdomen with a band of chevrons along its back and its spinnerets, the waist, and the cephalothorax with the fangs and two big eyes with shine in front, so the face reads at the head end alone.
function Spider:drawBody()
    local colors = Spider.colors
    local bob = math.sin(self.crawler.travelled / Spider.stride * math.pi * 4) * 1.5
    self:ellipse(-90, 4 + bob, 12, 10, 0.3, colors.fang, 5)
    self:ellipse(-48, 14 + bob, 92, 74, 0.3, colors.abdomen, 5)
    self:ellipse(-56, 26 + bob, 52, 30, 0.3, colors.body, 5, true)
    for index = 0, 2 do
        self:chevron(-30 - index * 18, 44 - index * 6 + bob, 16 - index * 3)
    end
    self:ellipse(-10, 4 + bob, 18, 14, 0, colors.body, 5)

    self:ellipse(16, 4 + bob, 62, 44, -0.12, colors.body, 6)
    self:ellipse(10, 14 + bob, 34, 14, -0.12, colors.light, 6, true)
    self:ellipse(40, -12 + bob, 12, 18, 0.5, colors.fang, 6)
    for _, eye in ipairs({{36, 14, 7}, {26, 20, 5}}) do
        local x, y = self:at(eye[1], eye[2] + bob)
        graphics2d.drawCircle(x, y, eye[3] + 1.5, colors.outline, {layer = 7})
        graphics2d.drawCircle(x, y, eye[3], colors.eye, {layer = 7})
        local shineX, shineY = self:at(eye[1] + eye[3] * 0.3, eye[2] + eye[3] * 0.35 + bob)
        graphics2d.drawCircle(shineX, shineY, eye[3] * 0.38, colors.shine, {layer = 7})
    end
end

-- A chevron of the band along the back of the abdomen, pointing to the head.
function Spider:chevron(along, lift, size)
    local tipX, tipY = self:at(along + size * 0.5 * self.facing, lift)
    local upperX, upperY = self:at(along - size * 0.4 * self.facing, lift + size * 0.45)
    local lowerX, lowerY = self:at(along - size * 0.4 * self.facing, lift - size * 0.45)
    graphics2d.drawStroke({upperX, upperY, tipX, tipY, lowerX, lowerY}, {width = size * 0.36, color = Spider.colors.band, join = 'round', cap = 'round', layer = 5})
end

-- Every leg grows from under the body: the legs of the far side draw darker behind it, and those of the near side over them and still under the body, so no leg crosses it.
function Spider:drawLeg(leg, color, layer)
    leg:draw(color, layer)
    for index = 2, leg.chain.count - 1 do
        local x, y = leg.chain:point(index)
        graphics2d.drawCircle(x, y, 3.5, Spider.colors.joint, {layer = layer})
    end
end

function Spider:draw(area)
    self.cave:draw()
    local colors = Spider.colors
    if self.target then
        local goalX, goalY = self.cave:project(self.target[1], self.target[2], 0)
        graphics2d.drawRing(goalX, goalY, 14, 3, colors.band, {layer = 3})
    end
    if self.thread then
        local color = m.color(colors.silk):withAlpha(0.7 * self.thread.life)
        local tipX, tipY = self:at(-92, 2)
        graphics2d.drawLine(self.thread.x, self.thread.y, tipX, tipY, 1.5, color, {layer = 2})
    end
    for _, leg in ipairs(self.legs) do
        local near = leg.layout.side == 'near'
        self:drawLeg(leg, near and colors.near or colors.far, near and 4 or 2)
        if self.showGuides then
            graphics2d.drawCircle(leg.restX, leg.restY, 4, '#FF6FDCA0', {layer = 30})
            graphics2d.drawCircle(leg.poleX, leg.poleY, 3, '#FF8FB0FF', {layer = 30})
        end
    end
    for _, palp in ipairs(self.palps) do
        local near = palp.side == 'near'
        self:drawLeg(palp, near and colors.near or colors.far, near and 4 or 2)
    end
    self:drawBody()
end

return Spider
