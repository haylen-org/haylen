-- Ceramic vases, glass panes and wooden crates that `physics2d.fracture` breaks into pieces when `world.onHit` reports an impact faster than what each one stands, hit by iron balls that a launcher throws along the arc that `world:predictPath` shows. The launcher throws its first balls by itself when the test opens.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local Look = require('categories.physics.look')
local PhysicsTest = require('categories.physics.physics-test')
local Scenery = require('categories.physics.scenery')

local Breakables = haylen.class('Breakables', PhysicsTest)

Breakables.code = [[
world.onHit = function(a, b, contact)
    if a == vase and contact.speed > 220 then physics2d.fracture(vase, {pieces = 7, impact = {contact.x, contact.y}}) end
end
local points, hit = world:predictPath(-720, 300, vx, vy, {steps = 120, radius = 20})]]

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
Breakables.launcher = {-720, 300}
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
    self.speed, self.seed, self.loaded = 900, 0, 0
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
    self.world = physics2d.newWorld()
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

-- The pieces take the material of the whole, so the shards of a vase stay glazed and the splinters of a crate stay wooden.
function Breakables:shatter(body, kind, x, y)
    self.seed = self.seed + 1
    for _, piece in ipairs(physics2d.fracture(body, {pieces = kind.pieces, impact = {x, y}, seed = self.seed, minimumArea = 30})) do
        Look.dress(piece, kind.material)
        self.pieces[#self.pieces + 1] = piece
    end
    while #self.pieces > Breakables.maxPieces do
        table.remove(self.pieces, 1):destroy()
    end
end

-- The velocity that reaches the target after a flight time set by the throw speed, so the ball lands where it aims whatever the gravity.
function Breakables:aimAt(x, y)
    local launcher = Breakables.launcher
    local dx, dy = x - launcher[1], y - launcher[2]
    local time = math.max(0.15, math.sqrt(dx * dx + dy * dy) / self.speed)
    return dx / time, dy / time - 0.5 * self.world.gravity.y * time
end

function Breakables:throw(x, y)
    if self.loaded > 0 then
        return
    end
    local vx, vy = self:aimAt(x, y)
    local launcher = Breakables.launcher
    local ball = self.world:createBody({x = launcher[1], y = launcher[2], vx = vx, vy = vy, bullet = true})
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
        graphics2d.drawPolygon(self.bandPoints, '#FFFFC93C', {layer = 1})
    end
    local sx, sy = x + (-18 * cos - -6 * sin), y + (-18 * sin + -6 * cos)
    graphics2d.drawShape({sx - 5, sy - 18, 10, 36}, {kind = 'ellipse', rotation = vase.rotation, color = material.light, layer = 1})
end

-- The launcher is a stone post with an iron cup on top, which holds the next ball until it is thrown.
function Breakables:drawLauncher()
    local launcher = Breakables.launcher
    local x, y = launcher[1], launcher[2]
    Look.block(x, (y + Breakables.ground) / 2 + 13, 50, Breakables.ground - y - 26, 'stone', 0)
    graphics2d.drawShape({x - 34, y - 26, 68, 52}, {kind = 'ellipse', startAngle = 0, sweep = math.pi, color = '#FF474F66', borderWidth = Look.outlinePixels * Look.pixel, borderColor = Look.ink, layer = 3})
    if self.loaded == 0 then
        Look.disc(x, y, Breakables.ballRadius, 'iron', 2)
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
    Look.drawAll(self.pieces, {layer = 1})
    Look.drawAll(self.balls, {layer = 2})
    self:drawLauncher()

    -- While the pointer aims, the predicted path of the ball shows as dots along its arc.
    if self.aiming and self.pointer.down then
        local vx, vy = self:aimAt(self.pointer.worldX, self.pointer.worldY)
        local launcher = Breakables.launcher
        local points = self.world:predictPath(launcher[1], launcher[2], vx, vy, {steps = 120, radius = Breakables.ballRadius})
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
