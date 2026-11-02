-- Objects of glass, wood and stone that `physics2d.fracture` breaks into Voronoi pieces around the point they are hit, by a tap or a click or by a wrecking ball that swings from the boom of a crawler crane.
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Burst = require('categories.physics.burst')
local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Fracture = haylen.class('Fracture', PhysicsTest)

Fracture.code = [[
local pieces = physics2d.fracture(body, {pieces = 10, impact = {x, y}, seed = 1})
world.onHit = function(a, b, contact)
    if contact.speed > 500 and a.valid then physics2d.fracture(a, {pieces = 10, impact = {contact.x, contact.y}}) end
end
local shape = world:pick(camera, pointerX, pointerY)[1]  -- The object under a tap.]]

Fracture.ground = 330
Fracture.breakSpeed = 500
Fracture.pivot = {520, -330}
-- The crane stands on its tracks at the right, with the foot of its boom on the cab.
Fracture.crane = 745
Fracture.boomFoot = {700, 190}
Fracture.window = Look.tint('glass', '#CC9BE7FF', '#CC6FB8D8', '#FFFFFFFF')
-- What each material breaks into: the material of its pieces and the effect of its break.
Fracture.materials = {
    glass = {pieces = 'glass', effect = 'ice'},
    wood = {pieces = 'wood', effect = 'crate'},
    woodCrate = {pieces = 'crate', effect = 'crate'},
    stone = {pieces = 'stone', effect = 'rock'},
}

function Fracture:enter()
    self:frame{
        code = Fracture.code,
        hint = 'Tap or click an object to break it where you hit it, or swing the wrecking ball. R or the X button rebuilds.',
        controls = {
            ui.label{text = 'Pieces of every break', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'pieces', value = 10, min = 2, max = 30, step = 1, showValue = true, decimals = 0, onChange = function(event) self.pieces = event.value end},
            ui.button{id = 'swing', text = 'Swing the wrecking ball', onClick = function() self:swing() end},
            ui.checkbox{id = 'outlines', text = 'Show the physics shapes', onChange = function(event) self.outlines = event.checked end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'swing',
    }
    self.pieces = 10
    self.seed = 1
    self.breaks = {
        ice = Burst('level/ice-break', {count = 3, scale = 0.6, layer = 30}),
        crate = Burst('level/crate-break', {count = 3, scale = 0.6, layer = 30}),
        rock = Burst('level/rock-break', {count = 3, scale = 0.6, layer = 30}),
    }
    self:build()
    timer.after(0.6, function() self:swing() end, {owner = self})
end

function Fracture:build()
    self.world = physics2d.newWorld({owner = self})
    self.whole, self.fragments = {}, 0
    local ground = self.world:createBody({type = 'static', x = 0, y = Fracture.ground + 30})
    ground:addBox(8000, 60)

    local bottom = Fracture.ground
    self:object(-150, bottom - 120, function(body) body:addBox(30, 240) end, 'glass')
    self:object(50, bottom - 120, function(body) body:addBox(30, 240) end, 'glass')
    self:object(-50, bottom - 258, function(body) body:addBox(300, 36) end, 'wood')
    self:object(300, bottom - 80, function(body) body:addPolygon({{-90, 80}, {-110, -10}, {-40, -80}, {60, -70}, {110, 20}, {70, 80}}) end, 'stone')
    for index = 0, 2 do
        self:object(-520 + index * 90, bottom - 40, function(body) body:addBox(80, 80) end, 'woodCrate')
    end
    self:object(-475, bottom - 121, function(body) body:addBox(80, 80) end, 'woodCrate')

    local pivot = self.world:createBody({type = 'static', x = Fracture.pivot[1], y = Fracture.pivot[2]})
    self.ball = self.world:createBody({x = Fracture.pivot[1], y = bottom - 90, bullet = true})
    self.ball:addCircle(50, {density = 20})
    Look.dress(self.ball, 'iron')
    self.world:createJoint('distance', pivot, self.ball, {ax = Fracture.pivot[1], ay = Fracture.pivot[2], bx = Fracture.pivot[1], by = bottom - 90})

    self.world.onHit = function(a, b, contact)
        if contact.speed > Fracture.breakSpeed then
            for _, body in ipairs({a, b}) do
                if body.valid and body.data and body.data.material then
                    self:shatter(body, contact.x, contact.y)
                end
            end
        end
    end
end

function Fracture:object(x, y, build, material)
    local body = self.world:createBody({x = x, y = y})
    build(body)
    Look.dress(body, material)
    body.data.material = material
    self.whole[#self.whole + 1] = body
end

function Fracture:shatter(body, x, y)
    self.seed = self.seed + 1
    local kind = Fracture.materials[body.data.material]
    local pieces = physics2d.fracture(body, {pieces = self.pieces, impact = {x, y}, seed = self.seed})
    for _, piece in ipairs(pieces) do
        Look.dress(piece, kind.pieces)
        self.whole[#self.whole + 1] = piece
    end
    self.fragments = self.fragments + #pieces
    self.breaks[kind.effect]:fire(x, y)
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
            if shape.body.data and shape.body.data.material then
                self:shatter(shape.body, self.pointer.worldX, self.pointer.worldY)
                break
            end
        end
    end
    for _, burst in pairs(self.breaks) do
        burst:update(dt)
    end
    local intact = 0
    for _, body in ipairs(self.whole) do
        intact = intact + ((body.valid and body.data.material) and 1 or 0)
    end
    self:status(string.format('%d objects stand whole, and the breaks made %d pieces.', intact, self.fragments))
end

function Fracture:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- The crane: a cab with a window and a counterweight on crawler tracks and a lattice boom of two chords with braces between them, whose tip holds the chain that hangs link after link down to the ball.
function Fracture:drawCrane()
    local px, py = Fracture.pivot[1], Fracture.pivot[2]
    local x, ground = Fracture.crane, Fracture.ground
    local unit = Look.pixel
    Look.block(x, ground - 24, 240, 44, 'tire', 0)
    for wheel = 0, 4 do
        Look.disc(x - 110 + wheel * 55, ground - 24, 14, 'steel', 0)
    end
    Look.block(x, ground - 96, 180, 100, 'yellow', 0)
    Look.block(x - 45, ground - 104, 70, 50, Fracture.window, 0)
    Look.block(x + 100, ground - 90, 70, 80, 'iron', 0)

    local fx, fy = Fracture.boomFoot[1], Fracture.boomFoot[2]
    local dx, dy = px - fx, py - fy
    local length = math.sqrt(dx * dx + dy * dy)
    local cos, sin = dx / length, dy / length
    local nx, ny = -sin * 12, cos * 12
    for _, side in ipairs({-1, 1}) do
        Look.line(fx + nx * side, fy + ny * side, px + nx * side, py + ny * side, 6 * unit, '#FFFFC93C', 0)
    end
    local braces = 12
    for index = 0, braces - 1 do
        local a, b = index / braces * length, (index + 1) / braces * length
        local side = index % 2 == 0 and 1 or -1
        Look.line(fx + cos * a + nx * side, fy + sin * a + ny * side, fx + cos * b - nx * side, fy + sin * b - ny * side, 4 * unit, '#FFE09A1E', 0)
    end
    Look.pin(fx, fy, 12 * unit, 0)

    local cx, cy = self.ball.x - px, self.ball.y - py
    local chain = math.sqrt(cx * cx + cy * cy)
    local links = math.floor(chain / 26)
    for index = 0, links - 1 do
        local t0, t1 = index / links, (index + 0.8) / links
        local width = index % 2 == 0 and 9 * unit or 5 * unit
        Look.line(px + cx * t0, py + cy * t0, px + cx * t1, py + cy * t1, width, '#FF8A96AE', 1)
    end
    Look.pin(px, py, 12 * unit, 3)
end

function Fracture:draw(area)
    Scenery.sky(self.camera, Fracture.ground)
    Scenery.ground(Fracture.ground)
    Look.drawAll(self.whole, {layer = 1})
    self:drawCrane()
    Look.draw(self.ball, {layer = 2})
    for _, burst in pairs(self.breaks) do
        burst:draw()
    end
    if self.outlines then
        self.world:debugDraw({layer = 20})
    end
end

return Fracture
