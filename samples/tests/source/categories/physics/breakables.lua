-- Ceramic vases, glass panes and wooden crates that `physics2d.fracture` breaks into pieces when `world.onHit` reports an impact faster than what each one stands, hit by iron balls that a mortar fires along the arc that `world:predictPath` shows. The mortar fires its first balls by itself when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Breakables = haylen.class('Breakables', PhysicsTest)

Breakables.code = [[
world.onHit = function(a, b, contact)
    if a == vase and contact.speed > 220 then
        physics2d.fracture(vase, {pieces = 7, impact = {contact.x, contact.y}})
    end
end
local points, hit = world:predictPath(muzzleX, muzzleY, vx, vy, {steps = 120, radius = 20})]]

-- Each kind breaks above its impact speed into its number of pieces, which keep the material of the whole.
Breakables.kinds = {
    glass = {speed = 160, pieces = 10, material = 'glass'},
    vase = {speed = 220, pieces = 7, material = Look.tint('teal', '#FF2FB3C4', '#FF1E8494', '#FF9BE9F2')},
    crate = {speed = 420, pieces = 5, material = 'woodCrate'},
}
Breakables.vase = {{-14, -60}, {14, -60}, {12, -50}, {30, -20}, {34, 10}, {22, 50}, {-22, 50}, {-34, 10}, {-30, -20}, {-12, -50}}
-- The painted bands of a vase, as the corners of each band in the coordinates of the vase.
Breakables.bands = {{{-31, -18}, {31, -18}, {33, -8}, {-33, -8}}, {{-33, 14}, {33, 14}, {30, 24}, {-30, 24}}}
Breakables.ground = 390
-- The mortar turns its barrel around this pivot and fires from its mouth, the length of the barrel ahead of the pivot.
Breakables.launcher = {-720, 300}
Breakables.barrel = {length = 96, width = 46, back = 26}
Breakables.bandColor = '#FFFFC93C'
Breakables.ballRadius = 20
Breakables.reload = 0.4
Breakables.maxBalls = 10
Breakables.maxPieces = 160
Breakables.firstThrows = {1, 2.4}

function Breakables:enter()
    self:frame{
        code = Breakables.code,
        hint = 'Hold on the stage to aim along the arc and let go to throw a ball. Glass breaks at the gentlest hit, vases at a harder one and crates only at a strong one. R or the X button sets everything up again.',
        controls = {
            ui.button{id = 'throw', text = 'Throw at the next target', onClick = function() self:throwAtTarget() end},
            ui.label{text = 'Throw speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 900, min = 500, max = 1500, step = 50, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Set up again', onClick = function() self:build() end},
        },
        focus = 'throw',
        view = {1520, 660},
    }
    self.camera.position = {-60, 100}
    self.speed, self.seed, self.loaded, self.aim = 900, 0, 0, -0.6
    self.vaseSkin = Breakables.smoothOutline(Breakables.vase, 4)
    self.vaseLit = {}
    self.vasePoints, self.litPoints, self.vaseOutline = {}, {}, {}
    for index, point in ipairs(self.vaseSkin) do
        self.vaseLit[index] = {point[1] * 0.8 - 4, point[2] * 0.9 - 3}
        self.vasePoints[index], self.litPoints[index] = {0, 0}, {0, 0}
        self.vaseOutline[index * 2 - 1], self.vaseOutline[index * 2] = 0, 0
    end
    self.bandPoints = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
    self.vaseStroke = {width = 1, color = Look.ink, join = 'round', closed = true, layer = 1}
    self.arc = {}
    self:build()
    for _, delay in ipairs(Breakables.firstThrows) do
        timer.after(delay, function() self:throwAtTarget() end, {owner = self})
    end
end

-- Returns a closed Catmull-Rom curve through the points of an outline with `steps` points for each of its sides.
function Breakables.smoothOutline(outline, steps)
    local count, curve = #outline, {}
    for index = 1, count do
        local p0, p1 = outline[(index - 2) % count + 1], outline[index]
        local p2, p3 = outline[index % count + 1], outline[(index + 1) % count + 1]
        for step = 0, steps - 1 do
            local t = step / steps
            local point = {}
            for axis = 1, 2 do
                point[axis] = 0.5 * (2 * p1[axis] + (p2[axis] - p0[axis]) * t + (2 * p0[axis] - 5 * p1[axis] + 4 * p2[axis] - p3[axis]) * t * t + (3 * p1[axis] - p0[axis] - 3 * p2[axis] + p3[axis]) * t * t * t)
            end
            curve[#curve + 1] = point
        end
    end
    return curve
end

function Breakables:build()
    self.world = physics2d.newWorld({owner = self})
    self.whole, self.vases, self.pieces, self.balls, self.lastImpact = {}, {}, {}, {}, 0

    local ground = self.world:createBody({type = 'static'})
    ground:addBox(4000, 40, {offsetY = Breakables.ground + 20})
    ground:addBox(280, 20, {offsetX = -150, offsetY = 240})
    ground:addBox(20, 140, {offsetX = -260, offsetY = 320})
    ground:addBox(20, 140, {offsetX = -40, offsetY = 320})

    for index = 0, 2 do
        local vase = self:object('vase', -230 + index * 80, 180)
        vase:addPolygon(Breakables.vase)
        self.vases[#self.vases + 1] = vase
    end
    for index = 0, 2 do
        self:object('glass', 140 + index * 80, 280):addBox(16, 220)
    end
    for row = 0, 2 do
        for column = 0, 2 - row do
            self:object('crate', 470 + column * 72 + row * 36, 355 - row * 70):addBox(70, 70)
        end
    end
    for _, body in ipairs(self.whole) do
        if body.data.kind ~= 'vase' then
            Look.dress(body, Breakables.kinds[body.data.kind].material)
        end
    end

    self.world.onHit = function(a, b, contact)
        self.lastImpact = math.max(self.lastImpact, contact.speed)
        self:check(a, contact)
        self:check(b, contact)
    end
end

function Breakables:object(kind, x, y)
    local body = self.world:createBody({x = x, y = y})
    body.data = {kind = kind}
    self.whole[#self.whole + 1] = body
    return body
end

-- A hit can name a body that an earlier hit of the same step already broke, which is gone by then.
function Breakables:check(body, contact)
    if not (body and body.valid) then
        return
    end
    local kind = body.data and Breakables.kinds[body.data.kind]
    if kind and contact.speed > kind.speed then
        self:shatter(body, kind, contact.x, contact.y)
    end
end

-- The pieces take the material of the whole, so the shards of a vase stay glazed and keep their share of its painted bands, and the splinters of a crate stay wooden.
function Breakables:shatter(body, kind, x, y)
    local vase = body.data.kind == 'vase'
    self.seed = self.seed + 1
    for _, piece in ipairs(physics2d.fracture(body, {pieces = kind.pieces, impact = {x, y}, seed = self.seed, minimumArea = 30})) do
        Look.dress(piece, kind.material)
        if vase then
            piece.data.bands = Breakables.bandsOf(piece)
        end
        self.pieces[#self.pieces + 1] = piece
    end
    while #self.pieces > Breakables.maxPieces do
        table.remove(self.pieces, 1):destroy()
    end
end

-- The pieces keep the space of the vase they come from, so the bands of the vase cut by the outline of a piece, a little inside it, are the bands the piece shows.
function Breakables.bandsOf(piece)
    local outline = {}
    for index, point in ipairs(piece:shapes()[1].points) do
        outline[index] = {point.x, point.y}
    end
    local inside = m.polygon.offset(outline, -3, {join = 'miter'})
    local bands = {}
    for _, band in ipairs(Breakables.bands) do
        for _, part in ipairs(m.polygon.intersect(inside, band)) do
            local corners, world = {}, {}
            for index, point in ipairs(part) do
                corners[index], world[index] = {point.x, point.y}, {0, 0}
            end
            bands[#bands + 1] = {corners = corners, world = world}
        end
    end
    return bands
end

-- Returns where a ball leaves the mouth of the barrel when the barrel points at `angle`.
function Breakables.muzzle(angle)
    local launcher, length = Breakables.launcher, Breakables.barrel.length
    return launcher[1] + math.cos(angle) * length, launcher[2] + math.sin(angle) * length
end

-- Returns the velocity that reaches the target from the mouth of the barrel after a flight time set by the throw speed, so the ball lands where it aims whatever the gravity, and the angle the barrel turns to for it. Turning the barrel moves its mouth, so a few rounds settle the velocity and the angle together.
function Breakables:aimAt(x, y)
    local launcher = Breakables.launcher
    local angle = math.atan(y - launcher[2], x - launcher[1])
    local vx, vy = 0, 0
    for _ = 1, 3 do
        local fromX, fromY = Breakables.muzzle(angle)
        local dx, dy = x - fromX, y - fromY
        local time = math.max(0.15, math.sqrt(dx * dx + dy * dy) / self.speed)
        vx, vy = dx / time, dy / time - 0.5 * self.world.gravity.y * time
        angle = math.atan(vy, vx)
    end
    return vx, vy, angle
end

function Breakables:throw(x, y)
    if self.loaded > 0 then
        return
    end
    local vx, vy, angle = self:aimAt(x, y)
    self.aim = angle
    local fromX, fromY = Breakables.muzzle(angle)
    local ball = self.world:createBody({x = fromX, y = fromY, vx = vx, vy = vy, bullet = true})
    ball:addCircle(Breakables.ballRadius, {density = 4, restitution = 0.3})
    Look.dress(ball, 'iron')
    self.balls[#self.balls + 1] = ball
    if #self.balls > Breakables.maxBalls then
        table.remove(self.balls, 1):destroy()
    end
    self.loaded = Breakables.reload
end

function Breakables:throwAtTarget()
    for _, body in ipairs(self.whole) do
        if body.valid then
            self:throw(body.x, body.y)
            return
        end
    end
end

function Breakables:update(dt)
    Breakables.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    self.loaded = math.max(0, self.loaded - dt)
    local pointer = self.pointer
    if pointer.pressed and not ui.usingPointer() then
        self.aiming = true
    end
    if self.aiming and pointer.down then
        local _, _, angle = self:aimAt(pointer.worldX, pointer.worldY)
        self.aim = angle
    end
    if self.aiming and pointer.released then
        self.aiming = false
        self:throw(pointer.worldX, pointer.worldY)
    end
    local whole = 0
    for _, body in ipairs(self.whole) do
        whole = whole + (body.valid and 1 or 0)
    end
    self:status(string.format('%d of the %d objects are whole, %d pieces lie around, and the hardest hit came at %d units per second.', whole, #self.whole, #self.pieces, math.floor(self.lastImpact + 0.5)))
end

function Breakables:fixedUpdate(step)
    self:simulate(self.world, step)
end

-- A vase is a smooth glazed body with two painted bands, its lit side toward the light and a tall shine, which turns with it.
function Breakables:drawVase(vase)
    local unit = Look.pixel
    local material = Breakables.kinds.vase.material
    local cos, sin = math.cos(vase.rotation), math.sin(vase.rotation)
    local x, y = vase.x, vase.y
    for index, point in ipairs(self.vaseSkin) do
        local world, lit, source = self.vasePoints[index], self.litPoints[index], self.vaseLit[index]
        world[1], world[2] = x + point[1] * cos - point[2] * sin, y + point[1] * sin + point[2] * cos
        lit[1], lit[2] = x + source[1] * cos - source[2] * sin, y + source[1] * sin + source[2] * cos
        self.vaseOutline[index * 2 - 1], self.vaseOutline[index * 2] = world[1], world[2]
    end
    self.vaseStroke.width = Look.outlinePixels * unit * 2
    graphics2d.drawStroke(self.vaseOutline, self.vaseStroke)
    graphics2d.drawPolygon(self.vasePoints, material.shade, {layer = 1})
    graphics2d.drawPolygon(self.litPoints, material.fill, {layer = 1})
    for _, band in ipairs(Breakables.bands) do
        for index, corner in ipairs(band) do
            local point = self.bandPoints[index]
            point[1], point[2] = x + corner[1] * cos - corner[2] * sin, y + corner[1] * sin + corner[2] * cos
        end
        graphics2d.drawPolygon(self.bandPoints, Breakables.bandColor, {layer = 1})
    end
    local sx, sy = x + (-18 * cos - -6 * sin), y + (-18 * sin + -6 * cos)
    graphics2d.drawShape({sx - 5, sy - 18, 10, 36}, {kind = 'ellipse', rotation = vase.rotation, color = material.light, layer = 1})
end

-- The mortar is a riveted iron barrel on a wooden carriage with two wheels, which turns toward its aim around a pivot on an iron mount and shows the next ball in its mouth until it fires.
function Breakables:drawLauncher()
    local launcher, barrel = Breakables.launcher, Breakables.barrel
    local x, y = launcher[1], launcher[2]
    local ground = Breakables.ground
    Look.block(x, ground - 30, 120, 36, 'darkWood', 0)
    for _, side in ipairs({-1, 1}) do
        Look.disc(x + side * 42, ground - 18, 18, 'darkWood', 1)
        Look.pin(x + side * 42, ground - 18, 6, 1)
    end
    Look.block(x, (y + ground - 48) / 2, 36, ground - 48 - y + 20, 'iron', 0)

    local cos, sin = math.cos(self.aim), math.sin(self.aim)
    local middle = (barrel.length - barrel.back) / 2
    Look.block(x + cos * middle, y + sin * middle, barrel.length + barrel.back, barrel.width, 'iron', 2, self.aim)
    local mouth = barrel.length - 8
    Look.block(x + cos * mouth, y + sin * mouth, 16, barrel.width + 10, 'steel', 3, self.aim)
    Look.block(x - cos * (barrel.back - 6), y - sin * (barrel.back - 6), 14, barrel.width - 6, 'steel', 3, self.aim)
    if self.loaded == 0 then
        local ballX, ballY = Breakables.muzzle(self.aim)
        Look.disc(ballX, ballY, Breakables.ballRadius, 'iron', 1)
    end
    Look.pin(x, y, 9, 3)
end

-- A shard of a vase draws its share of the painted bands over its glaze, turning with it.
function Breakables.drawBands(piece)
    local cos, sin = math.cos(piece.rotation), math.sin(piece.rotation)
    local x, y = piece.x, piece.y
    for _, band in ipairs(piece.data.bands) do
        for index, corner in ipairs(band.corners) do
            local point = band.world[index]
            point[1], point[2] = x + corner[1] * cos - corner[2] * sin, y + corner[1] * sin + corner[2] * cos
        end
        graphics2d.drawPolygon(band.world, Breakables.bandColor, {layer = 1})
    end
end

function Breakables:draw(area)
    Scenery.sky(self.camera, Breakables.ground)
    Scenery.ground(Breakables.ground)
    Look.block(-150, 240, 280, 20, 'darkWood', 0)
    Look.block(-260, 320, 20, 140, 'darkWood', 0)
    Look.block(-40, 320, 20, 140, 'darkWood', 0)
    for _, body in ipairs(self.whole) do
        if body.valid and body.data.kind == 'vase' then
            self:drawVase(body)
        end
    end
    Look.drawAll(self.whole, {layer = 1})
    for _, piece in ipairs(self.pieces) do
        Look.draw(piece, {layer = 1})
        if piece.valid and piece.data.bands then
            Breakables.drawBands(piece)
        end
    end
    Look.drawAll(self.balls, {layer = 2})
    self:drawLauncher()

    -- While the pointer aims, the predicted path of the ball shows as dots along its arc.
    if self.aiming and self.pointer.down then
        local vx, vy, angle = self:aimAt(self.pointer.worldX, self.pointer.worldY)
        local fromX, fromY = Breakables.muzzle(angle)
        local points = self.world:predictPath(fromX, fromY, vx, vy, {steps = 120, radius = Breakables.ballRadius})
        local arc = self.arc
        local count = 0
        for index = 1, #points, 3 do
            arc[count * 2 + 1], arc[count * 2 + 2] = points[index].x, points[index].y
            count = count + 1
        end
        graphics2d.drawPoints(arc, {radius = 5, color = '#E6FFC93C', count = count, layer = 4})
    end
end

return Breakables
