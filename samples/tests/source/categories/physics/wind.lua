-- Fans as directional force fields that push every body inside them with the same force, so light leaves fly and a heavy crate stays, gusts that change their strength over time and a vortex field that swirls what the updraft lifts. The fans hang on stone walls and stand on the floor of a yard.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local Grab = require('categories.physics.grab')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Wind = haylen.class('Wind', PhysicsTest)

Wind.code = [[
local fan = physics2d.newForceField(world, {kind = 'directional', x = -175, y = 320, width = 1030, height = 140, direction = {1, 0}, strength = 14, acceleration = false})
fan.strength = 14 * (1 + 0.9 * noise:perlin(time * 0.6, 7))  -- A gust.
physics2d.newForceField(world, {kind = 'vortex', x = 450, y = -140, radius = 150, strength = 7, acceleration = false})
leaf:addBox(34, 10, {density = 0.1})  -- The same force throws a leaf far and leaves a crate still.]]

Wind.ground = 390
Wind.wall = 790
-- Each fan blows through a rectangle in front of its housing, given by the center, size and direction of the rectangle and the center of the housing.
Wind.fans = {
    {x = -175, y = 320, width = 1030, height = 140, direction = {1, 0}, housing = {-735, 320}, group = 'floor'},
    {x = 625, y = 320, width = 130, height = 140, direction = {-1, 0}, housing = {735, 320}, group = 'floor'},
    {x = 450, y = 95, width = 200, height = 590, direction = {0, -1}, housing = {450, 390}, group = 'updraft'},
}
Wind.vortexSpot = {450, -140, 150}
Wind.leafColors = {{'#FF7BC74D', '#FF4E9A32'}, {'#FFFFC93C', '#FFD99A12'}, {'#FFFF9A3C', '#FFD06E1E'}, {'#FFF0473E', '#FFB9302A'}}
Wind.maxLeaves = 40
Wind.cardboard = Look.tint('orange', '#FFE0B47C', '#FFB98A50', '#FFF5D5A5')

function Wind:enter()
    self:frame{
        code = Wind.code,
        hint = 'Fans push every body with the same force: leaves fly, balloons drift, the cardboard box slides in the gusts and the crate never moves. Drag anything around. R or the X button rebuilds.',
        controls = {
            ui.checkbox{id = 'floor', text = 'Floor fans', checked = true, onChange = function(event) self:switch('floor', event.checked) end},
            ui.checkbox{id = 'updraft', text = 'Updraft fan', checked = true, onChange = function(event) self:switch('updraft', event.checked) end},
            ui.checkbox{id = 'vortex', text = 'Vortex', checked = true, onChange = function(event) self:switch('vortex', event.checked) end},
            ui.toggle{id = 'gusts', text = 'Gusts', checked = true, onChange = function(event) self.gusts = event.checked end},
            ui.label{text = 'Fan force', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'force', value = 14, min = 0, max = 60, step = 1, showValue = true, decimals = 0, onChange = function(event) self.force = event.value end},
            ui.button{id = 'leaves', text = 'Drop leaves', onClick = function() self:dropLeaves(6) end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'floor',
        view = {1640, 720},
    }
    self.camera.position = {0, 40}
    self.force, self.gusts, self.time = 14, true, 0
    self.enabled = {floor = true, updraft = true, vortex = true}
    self.noise = m.noise(50)
    self.random = m.random(50)
    self.string = {0, 0, 0, 0, 0, 0, 0, 0}
    self.stringStroke = {width = 1, color = '#E6FFFFFF', join = 'round', cap = 'round', layer = 1}
    self.gust = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
    self.gustStroke = {width = 1, color = '#FFFFFFFF', join = 'round', cap = 'round', layer = 5}
    self.leafShape = {kind = 'ellipse', rotation = 0, color = '#FFFFFFFF', borderWidth = 0, borderColor = Look.ink, layer = 2}
    self.leafRect = {0, 0, 0, 0}
    self:build()
end

function Wind:build()
    self.world = physics2d.newWorld()
    self.grab = Grab(self.world)
    self.bodies, self.leaves, self.balloons = {}, {}, {}

    local yard = self.world:createBody({type = 'static'})
    yard:addBox(4000, 40, {offsetY = Wind.ground + 20})
    yard:addBox(1600, 20, {offsetY = -430})
    yard:addBox(40, 900, {offsetX = -Wind.wall - 20, offsetY = 0})
    yard:addBox(40, 900, {offsetX = Wind.wall + 20, offsetY = 0})

    self.fanList = {}
    for _, spec in ipairs(Wind.fans) do
        local field = physics2d.newForceField(self.world, {kind = 'directional', x = spec.x, y = spec.y, width = spec.width, height = spec.height, direction = spec.direction, strength = self.force, acceleration = false, enabled = self.enabled[spec.group]})
        self.fanList[#self.fanList + 1] = {field = field, spec = spec, spin = 0}
    end
    local spot = Wind.vortexSpot
    self.vortex = physics2d.newForceField(self.world, {kind = 'vortex', x = spot[1], y = spot[2], radius = spot[3], strength = self.force / 2, acceleration = false, enabled = self.enabled.vortex})

    local crate = self.world:createBody({x = -560, y = Wind.ground - 32})
    crate:addBox(64, 64)
    Look.dress(crate, 'woodCrate')
    local box = self.world:createBody({x = -400, y = Wind.ground - 30})
    box:addBox(60, 60, {density = 0.05})
    Look.dress(box, Wind.cardboard)
    box.data.cardboard = true
    local ball = self.world:createBody({x = -250, y = Wind.ground - 30})
    ball:addCircle(30, {density = 0.08, restitution = 0.4})
    Look.dress(ball, 'teal', {spin = true})
    self.bodies = {crate, box, ball}
    for index = 1, 4 do
        local balloon = self.world:createBody({x = -500 + index * 150, y = -200, gravityScale = 0.1, linearDamping = 0.8})
        balloon:addCircle(26, {density = 0.05})
        balloon.data = {balloon = Look.toys[index + 1]}
        self.balloons[#self.balloons + 1] = balloon
    end
    self:dropLeaves(14)
end

function Wind:dropLeaves(count)
    for _ = 1, count do
        local leaf = self.world:createBody({x = self.random:range(-700, 300), y = self.random:range(-350, 100), rotation = self.random:range(0, math.pi), linearDamping = 3, angularDamping = 1.5})
        leaf:addBox(34, 10, {density = 0.1})
        leaf.data = {colors = Wind.leafColors[self.random:integer(1, #Wind.leafColors)]}
        self.leaves[#self.leaves + 1] = leaf
        if #self.leaves > Wind.maxLeaves then
            table.remove(self.leaves, 1):destroy()
        end
    end
end

function Wind:switch(group, enabled)
    self.enabled[group] = enabled
    if group == 'vortex' then
        self.vortex.enabled = enabled
        return
    end
    for _, fan in ipairs(self.fanList) do
        if fan.spec.group == group then
            fan.field.enabled = enabled
        end
    end
end

function Wind:update(dt)
    Wind.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.grab:update(self.pointer, self.camera)
    local floor, updraft = self.fanList[1].field, self.fanList[3].field
    local blowing = floor.enabled and string.format('The floor fan blows with a force of %d on %d bodies', math.floor(floor.strength + 0.5), floor.bodyCount) or 'The floor fans are off'
    local lifting = updraft.enabled and string.format('the updraft lifts %d', updraft.bodyCount) or 'the updraft is off'
    local swirling = self.vortex.enabled and string.format('the vortex swirls %d', self.vortex.bodyCount) or 'the vortex is off'
    self:status(string.format('%s, %s and %s.', blowing, lifting, swirling))
end

-- Gusts move the strength of every fan along its own line of noise.
function Wind:fixedUpdate(step)
    self.time = self.time + step
    for index, fan in ipairs(self.fanList) do
        local gust = self.gusts and 1 + 0.9 * self.noise:perlin(self.time * 0.6, index * 7) or 1
        fan.field.strength = self.force * gust
        fan.spin = fan.spin + (fan.field.enabled and step * (4 + fan.field.strength * 0.4) or 0)
    end
    self.vortex.strength = self.force / 2
    self:simulate(self.world, step)
end

-- Gusts of wind are short wavy strokes that drift along a fan, faster when it blows harder, and fade in and out over their way.
function Wind:drawGusts(fan)
    local spec, field = fan.spec, fan.field
    local dx, dy = spec.direction[1], spec.direction[2]
    local length = math.abs(dx) > 0 and spec.width or spec.height
    local across = math.abs(dx) > 0 and spec.height or spec.width
    local unit = Look.pixel
    local stroke, gust = self.gustStroke, self.gust
    stroke.width = 5 * unit
    for lane = 1, 4 do
        local side = (lane / 5 - 0.5) * across
        local travel = self.time * (120 + field.strength * 10) / length + lane * 0.37
        local phase = travel % 1
        local along = phase * length - length / 2
        local alpha = math.sin(phase * math.pi) * math.min(0.9, 0.45 + field.strength / 40)
        for point = 0, 4 do
            local ahead = along + point * 18
            local wave = math.sin(point * 1.4 + self.time * 6 + lane) * 6
            gust[point * 2 + 1] = spec.x + dx * ahead - dy * (side + wave)
            gust[point * 2 + 2] = spec.y + dy * ahead + dx * (side + wave)
        end
        stroke.color = {1, 1, 1, alpha}
        graphics2d.drawStroke(gust, stroke)
    end
end

-- A box fan in an iron housing with a round guard and three blades that spin with the strength of its wind, on a bracket of the wall or a base on the floor.
function Wind:drawFan(fan)
    local spec, field = fan.spec, fan.field
    local unit = Look.pixel
    local x, y = spec.housing[1], spec.housing[2]
    if spec.direction[2] ~= 0 then
        graphics2d.drawShape({x - 100, y - 10, 200, 26}, {radius = 6, color = '#FF15172A', borderWidth = 3 * unit, borderColor = Look.ink, layer = 2})
        for blade = -3, 3 do
            local width = 10 + 8 * math.abs(math.sin(fan.spin + blade))
            graphics2d.drawRect({x + blade * 26 - width / 2, y - 6, width, 18}, field.enabled and '#FF7088A6' or '#FF474F66', {layer = 2})
        end
        for slat = -4, 4 do
            Look.line(x + slat * 22, y - 10, x + slat * 22, y + 14, 3, '#FF9AA5BE', 2)
        end
        Look.line(x - 100, y - 10, x + 100, y - 10, 4, '#FF9AA5BE', 2)
        return
    end
    local side = spec.direction[1] > 0 and -1 or 1
    Look.block(x + side * 40, y, 20, 70, 'iron', 1)
    Look.block(x, y, 90, 110, 'iron', 2)
    graphics2d.drawCircle(x, y, 38, '#FF2F3352', {layer = 2})
    for blade = 0, 2 do
        local angle = fan.spin + blade * math.pi * 2 / 3
        local bx, by = x + math.cos(angle) * 18, y + math.sin(angle) * 18
        graphics2d.drawShape({bx - 18, by - 8, 36, 16}, {kind = 'ellipse', rotation = angle, color = field.enabled and '#FFC9D3E2' or '#FF7088A6', borderWidth = 2 * unit, borderColor = Look.ink, layer = 2})
    end
    Look.disc(x, y, 9, 'steel', 2)
    graphics2d.drawRing(x, y, 38, 3 * unit, '#FF9AA5BE', {layer = 2})
    graphics2d.drawRing(x, y, 24, 2 * unit, '#B39AA5BE', {layer = 2})
end

-- The vortex is a spiral of motes along two arms that turn around its middle and fade toward its edge.
function Wind:drawVortex()
    if not self.vortex.enabled then
        return
    end
    local spot = Wind.vortexSpot
    for arm = 0, 1 do
        for mote = 1, 14 do
            local share = mote / 14
            local angle = -self.time * 2.4 + share * 4.2 + arm * math.pi
            local radius = spot[3] * share
            graphics2d.drawCircle(spot[1] + math.cos(angle) * radius, spot[2] + math.sin(angle) * radius, 2 + share * 4, {1, 1, 1, 0.7 * (1 - share * 0.8)}, {layer = 5})
        end
    end
end

-- A balloon is a shiny oval with a knot under it and a string that trails behind its motion.
function Wind:drawBalloon(balloon)
    local unit = Look.pixel
    local material = balloon.data.balloon
    local x, y = balloon.x, balloon.y
    local velocity = balloon.velocity
    local path = self.string
    path[1], path[2] = x, y + 30
    for point = 1, 3 do
        local drag = point / 3
        path[point * 2 + 1] = x - velocity.x * 0.06 * drag + math.sin(self.time * 3 + point + x * 0.01) * 6 * drag
        path[point * 2 + 2] = y + 30 + point * 26
    end
    self.stringStroke.width = 2.5 * unit
    graphics2d.drawStroke(path, self.stringStroke)
    graphics2d.drawShape({x - 6, y + 22, 12, 10}, {startAngle = -math.pi * 0.75, sweep = math.pi / 2, color = material.shade, layer = 1})
    graphics2d.drawShape({x - 27, y - 32, 54, 62}, {kind = 'ellipse', color = material.shade, borderWidth = Look.outlinePixels * unit, borderColor = Look.ink, layer = 1})
    graphics2d.drawShape({x - 23, y - 30, 46, 54}, {kind = 'ellipse', color = material.fill, layer = 1})
    graphics2d.drawShape({x - 16, y - 22, 10, 18}, {kind = 'ellipse', rotation = 0.4, color = material.light, layer = 1})
end

-- A leaf is a pointed oval in two tones with a midrib, which turns with its body.
function Wind:drawLeaf(leaf)
    local unit = Look.pixel
    local colors = leaf.data.colors
    local shape, rect = self.leafShape, self.leafRect
    local angle = leaf.rotation
    rect[1], rect[2], rect[3], rect[4] = leaf.x - 20, leaf.y - 8, 40, 16
    shape.rotation, shape.color, shape.borderWidth = angle, colors[2], 2.5 * unit
    graphics2d.drawShape(rect, shape)
    rect[1], rect[2], rect[3], rect[4] = leaf.x - 18.5, leaf.y - 6, 34, 9
    shape.color, shape.borderWidth = colors[1], 0
    graphics2d.drawShape(rect, shape)
    local cos, sin = math.cos(angle), math.sin(angle)
    graphics2d.drawLine(leaf.x - cos * 15, leaf.y - sin * 15, leaf.x + cos * 15, leaf.y + sin * 15, 1.5 * unit, colors[2], {layer = 2})
end

function Wind:draw(area)
    Scenery.sky(self.camera, Wind.ground)
    Scenery.ground(Wind.ground)
    for _, side in ipairs({-1, 1}) do
        Look.block(side * (Wind.wall + 30), Wind.ground - 450, 60, 900, 'stone', 0)
    end
    for _, fan in ipairs(self.fanList) do
        if fan.field.enabled then
            self:drawGusts(fan)
        end
        self:drawFan(fan)
    end
    self:drawVortex()
    for _, balloon in ipairs(self.balloons) do
        self:drawBalloon(balloon)
    end
    Look.drawAll(self.bodies, {layer = 1})
    local box = self.bodies[2]
    graphics2d.drawShape({box.x - 30, box.y - 6, 60, 12}, {rotation = box.rotation, color = '#B3C98E4A', layer = 1})
    for _, leaf in ipairs(self.leaves) do
        self:drawLeaf(leaf)
    end
    self.grab:draw()
end

return Wind
