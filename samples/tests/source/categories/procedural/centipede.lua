-- Centipede: a centipede that crawls around a generated cave, up walls, across the ceiling and around rocks. Its head walks the surface at a fixed height, its body is one chain that follows the head with a bend limit and whose points are put back on the surface every frame, and a pair of legs on every joint of the body reaches the rock and steps in two alternating groups.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Cave = require('categories.procedural.cave')
local Crawler = require('categories.procedural.crawler')
local Leg = require('categories.procedural.leg')

local Centipede = haylen.class('Centipede', AlgorithmTest)

Centipede.code = [[
local body = animation2d.newChain({x = head.x, y = head.y, segments = 26, length = 17, maxBend = 0.45})
body:follow(head.x, head.y)  -- Every point trails the one before it.
for index = 2, body.count do
    local x, y = body:point(index)
    body:setPoint(index, cave:project(x, y, height))  -- Back on the rock around corners.
end]]

Centipede.height = 20
Centipede.stride = 12
Centipede.view = {1300, 700}
Centipede.colors = {light = '#FFC8643F', dark = '#FF94402B', outline = '#FF2A130C', leg = '#FFE0A060', far = '#FF8A5530', eye = '#FF141018', antenna = '#FFE0A060'}

function Centipede:enter()
    self.seed = 11
    self.speed = 150
    self.segments = 26
    self.showPoints = false
    self:build()
    self:frame{
        view = Centipede.view,
        code = Centipede.code,
        hint = 'Click or tap a point and the centipede crawls to the nearest spot of its rock. Without a point it wanders. R or the X button makes a new cave.',
        controls = {
            ui.formField{label = 'Speed', ui.slider{id = 'speed', min = 40, max = 360, step = 10, value = self.speed, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end}},
            ui.formField{label = 'Segments of the body', ui.slider{id = 'segments', min = 8, max = 44, step = 1, value = self.segments, showValue = true, decimals = 0, onChange = function(event)
                self.segments = math.tointeger(event.value)
                self:buildBody()
            end}},
            ui.checkbox{id = 'points', text = 'Show the points of the chain', checked = false, onChange = function(event) self.showPoints = event.checked end},
            ui.button{id = 'wander', text = 'Let it wander', onClick = function() self.target = nil end},
            ui.button{id = 'cave', text = 'New cave', variant = 'primary', onClick = function() self:newCave() end},
        },
    }
end

function Centipede:newCave()
    self.seed = self.seed + 1
    self:build()
end

function Centipede:build()
    self.cave = Cave(self.seed, 1240, 640)
    self.crawler = Crawler(self.cave, -200, 400, Centipede.height, self.speed, self.seed)
    self.target = nil
    self:buildBody()
end

-- Lays the body out behind the head with a pair of legs on every joint, and two antennae on the head.
function Centipede:buildBody()
    local crawler = self.crawler
    local tx, ty = crawler:tangent()
    self.body = animation2d.newChain({x = crawler.x, y = crawler.y, segments = self.segments, length = 17, angle = math.atan(-ty, -tx), maxBend = 0.45})
    self.legs = {}
    for index = 2, self.body.count - 1 do
        for _, side in ipairs({'near', 'far'}) do
            local leg = Leg({x = crawler.x, y = crawler.y, lengths = {11, 13}, group = (index + (side == 'near' and 0 or 1)) % 2 + 1, stepTime = 0.07, lift = 7, widths = {4, 3, 2}})
            leg.joint, leg.side = index, side
            self.legs[#self.legs + 1] = leg
        end
    end
    self.antennae = {}
    for _ = 1, 2 do
        self.antennae[#self.antennae + 1] = Leg({x = crawler.x, y = crawler.y, lengths = {18, 16}, widths = {3, 2.5, 1.5}, color = Centipede.colors.antenna, layer = 6})
    end
    self:placeBody()
    for _, leg in ipairs(self.legs) do
        leg:place(leg.restX, leg.restY)
    end
end

-- Pulls the body after the head and puts every point back on the rock, then sets the hip, the rest point, the lead and the pole of every leg from the joint it grows from.
function Centipede:placeBody()
    local cave, body = self.cave, self.body
    body:follow(self.crawler.x, self.crawler.y)
    for index = 2, body.count do
        local x, y = body:point(index)
        body:setPoint(index, cave:project(x, y, Centipede.height))
    end
    local lead = self.crawler.moved / math.max(self.dt or 1 / 60, 1e-4) * 0.07
    for _, leg in ipairs(self.legs) do
        local x, y = body:point(leg.joint)
        local aheadX, aheadY = body:point(leg.joint - 1)
        local tx, ty = aheadX - x, aheadY - y
        local length = math.max(1e-4, math.sqrt(tx * tx + ty * ty))
        tx, ty = tx / length, ty / length
        local nx, ny = cave:normal(x, y)
        local reach = leg.side == 'near' and 7 or -5
        leg.hipX, leg.hipY = x - nx * 5, y - ny * 5
        leg.restX, leg.restY = cave:project(x - nx * Centipede.height + tx * reach, y - ny * Centipede.height + ty * reach, 0)
        leg.leadX, leg.leadY = tx * lead, ty * lead
        -- The knee points forward along the body, so each leg folds under it.
        leg.poleX, leg.poleY = leg.hipX + tx * 14 + nx * 2, leg.hipY + ty * 14 + ny * 2
        leg.nx, leg.ny = nx, ny
    end
end

function Centipede:update(dt)
    Centipede.super.update(self, dt)
    if not self.stage then
        return
    end
    if input.pressed('reset') then
        self:newCave()
    end
    if self.pointer.pressed then
        self.target = {self.pointer.worldX, self.pointer.worldY}
    end

    local crawler = self.crawler
    crawler.speed = self.speed
    crawler:update(dt, self.target and self.target[1], self.target and self.target[2])
    self.dt = dt
    self:placeBody()
    Leg.gait(self.legs, Centipede.stride, 0, 0, function(x, y) return self.cave:project(x, y, 0) end)
    local stepping = 0
    for _, leg in ipairs(self.legs) do
        leg:update(dt, leg.hipX, leg.hipY, leg.nx, leg.ny, leg.poleX, leg.poleY)
        stepping = stepping + (leg:stepping() and 1 or 0)
    end

    -- The antennae reach ahead of the head and sway, each to its own side.
    local nx, ny = crawler.nx, crawler.ny
    local tx, ty = crawler:tangent()
    local time = haylen.elapsed()
    for index, antenna in ipairs(self.antennae) do
        local spread = (index == 1 and 1 or -1) * 10 + math.sin(time * 5 + index * 2) * 8
        local hipX, hipY = crawler.x + tx * 10 + nx * 4, crawler.y + ty * 10 + ny * 4
        antenna:place(hipX + tx * 28 + nx * (14 + spread), hipY + ty * 28 + ny * (14 + spread))
        antenna:update(dt, hipX, hipY, nx, ny, hipX + tx * 10 + nx * 40, hipY + ty * 10 + ny * 40)
    end
    local state = self.target and (crawler.arrived and 'arrived' or 'crawling to the point') or 'wandering'
    self:status(string.format('The centipede is %s, %d segments, legs stepping %d of %d, cave %d', state, self.segments, stepping, #self.legs, self.seed))
end

function Centipede:draw(area)
    self.cave:draw()
    local colors = Centipede.colors
    if self.target then
        local goalX, goalY = self.cave:project(self.target[1], self.target[2], 0)
        graphics2d.drawRing(goalX, goalY, 12, 3, '#FFF2B23A', {layer = 3})
    end
    for _, leg in ipairs(self.legs) do
        local near = leg.side == 'near'
        leg:draw(near and colors.leg or colors.far, near and 6 or 3)
    end
    for _, antenna in ipairs(self.antennae) do
        antenna:draw()
    end

    -- The body draws from the tail to the head, so each segment overlaps the one behind it.
    local body = self.body
    for index = body.count, 1, -1 do
        local x, y = body:point(index)
        local angle = body:angle(math.min(index, body.count - 1)) + math.pi
        local taper = index == 1 and 1.2 or 1 - (index / body.count) * 0.25
        local width, height = 28 * taper, 20 * taper
        graphics2d.drawShape({x - width / 2, y - height / 2, width, height}, {kind = 'ellipse', rotation = angle, color = index % 2 == 0 and colors.dark or colors.light, borderWidth = 2.5, borderColor = colors.outline, layer = 5})
        if self.showPoints then
            graphics2d.drawRing(x, y, 4, 1.5, '#FF8FB0FF', {layer = 8})
        end
    end
    local crawler = self.crawler
    local tx, ty = crawler:tangent()
    graphics2d.drawCircle(crawler.x + tx * 8 + crawler.nx * 5, crawler.y + ty * 8 + crawler.ny * 5, 3.5, colors.eye, {layer = 5})
end

return Centipede
