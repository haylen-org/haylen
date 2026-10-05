-- Crates, vases and glass panes that `physics2d.fracture` breaks into pieces when `world.onHit` reports an impact faster than what each one stands, hit by balls thrown along the arc that `world:predictPath` shows.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Breakables = haylen.class('Breakables', PhysicsTest)

-- Each kind breaks above its impact speed into its number of pieces.
local kKinds = {
    glass = {speed = 160, pieces = 10, color = '#9981D4FA'},
    vase = {speed = 220, pieces = 7, color = '#FF4DB6AC'},
    crate = {speed = 420, pieces = 5, color = '#FFBC8F5A'},
}
local kVase = {{-14, -60}, {14, -60}, {12, -50}, {30, -20}, {34, 10}, {22, 50}, {-22, 50}, {-34, 10}, {-30, -20}, {-12, -50}}
local kLauncher = {-720, 300}
local kBallRadius = 20
local kMaxBalls = 10
local kMaxPieces = 160

function Breakables:enter()
    self:frame{
        hint = 'Hold on the stage to aim along the arc and let go to throw a ball. Glass breaks at the gentlest hit, vases at a harder one and crates only at a strong one. R or the X button sets everything up again.',
        controls = {
            ui.button{id = 'throw', text = 'Throw at the next target', onClick = function() self:throwAtTarget() end},
            ui.label{text = 'Throw speed', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'speed', value = 900, min = 500, max = 1500, step = 50, showValue = true, decimals = 0, onChange = function(event) self.speed = event.value end},
            ui.button{id = 'reset', text = 'Set up again', onClick = function() self:build() end},
        },
        focus = 'throw',
    }
    self.speed, self.seed = 900, 0
    self:build()
end

function Breakables:build()
    self.world = physics2d.newWorld()
    self.whole, self.pieces, self.balls, self.lastImpact = {}, {}, {}, 0

    local ground = self.world:createBody({type = 'static'})
    parts.box(ground, 1600, 40, {offsetY = 410})
    parts.box(ground, 280, 20, {offsetX = -150, offsetY = 240})
    parts.box(ground, 20, 140, {offsetX = -260, offsetY = 320})
    parts.box(ground, 20, 140, {offsetX = -40, offsetY = 320})
    self.statics = {ground}

    for index = 0, 2 do
        self:object('vase', -230 + index * 80, 180, function(body) parts.polygon(body, kVase) end)
    end
    for index = 0, 2 do
        self:object('glass', 140 + index * 80, 280, function(body) parts.box(body, 16, 220) end)
    end
    for row = 0, 2 do
        for column = 0, 2 - row do
            self:object('crate', 470 + column * 72 + row * 36, 355 - row * 70, function(body) parts.box(body, 70, 70) end)
        end
    end

    self.world.onHit = function(a, b, contact)
        self.lastImpact = math.max(self.lastImpact, contact.speed)
        for _, body in ipairs({a, b}) do
            local kind = body.data and kKinds[body.data.kind]
            if kind and contact.speed > kind.speed then
                self:shatter(body, kind, contact.x, contact.y)
            end
        end
    end
end

function Breakables:object(kind, x, y, build)
    local body = self.world:createBody({x = x, y = y})
    build(body)
    parts.paint(body, kKinds[kind].color)
    body.data.kind = kind
    self.whole[#self.whole + 1] = body
end

-- The pieces carry the outlines of their shapes into the parts drawing, so they draw filled like whole objects.
function Breakables:shatter(body, kind, x, y)
    self.seed = self.seed + 1
    for _, piece in ipairs(physics2d.fracture(body, {pieces = kind.pieces, impact = {x, y}, seed = self.seed, minimumArea = 30})) do
        for _, shape in ipairs(piece:shapes()) do
            local outline = {}
            for _, point in ipairs(shape.points) do
                outline[#outline + 1] = {point.x, point.y}
            end
            parts.outline(piece, outline)
        end
        parts.paint(piece, kind.color)
        self.pieces[#self.pieces + 1] = piece
    end
    while #self.pieces > kMaxPieces do
        table.remove(self.pieces, 1):destroy()
    end
end

-- The velocity that reaches the target after a flight time set by the throw speed, so the ball lands where it aims whatever the gravity.
function Breakables:aimAt(x, y)
    local dx, dy = x - kLauncher[1], y - kLauncher[2]
    local time = math.max(0.15, math.sqrt(dx * dx + dy * dy) / self.speed)
    return dx / time, dy / time - 0.5 * self.world.gravity.y * time
end

function Breakables:throw(x, y)
    local vx, vy = self:aimAt(x, y)
    local ball = self.world:createBody({x = kLauncher[1], y = kLauncher[2], vx = vx, vy = vy, bullet = true})
    parts.circle(ball, kBallRadius, {density = 4, restitution = 0.3})
    parts.paint(ball, '#FF607D8B')
    self.balls[#self.balls + 1] = ball
    if #self.balls > kMaxBalls then
        table.remove(self.balls, 1):destroy()
    end
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
    local pointer = self.pointer
    if pointer.released then
        self:throw(pointer.worldX, pointer.worldY)
    end
    local whole = 0
    for _, body in ipairs(self.whole) do
        whole = whole + (body.valid and 1 or 0)
    end
    self:status(string.format('Whole objects %d, pieces %d, fastest impact %.0f, step %.2f ms', whole, #self.pieces, self.lastImpact, self:stepTime()))
end

function Breakables:fixedUpdate(step)
    self:simulate(self.world, step)
end

function Breakables:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.whole, {layer = 1})
    parts.drawAll(self.pieces, {layer = 1})
    parts.drawAll(self.balls, {layer = 2})
    graphics2d.drawCircle(kLauncher[1], kLauncher[2], 36, '#FF455A64', {layer = 3})
    graphics2d.drawRing(kLauncher[1], kLauncher[2], 36, 6, '#FF90A4AE', {layer = 3})
    if self.pointer.down then
        local vx, vy = self:aimAt(self.pointer.worldX, self.pointer.worldY)
        local points = self.world:predictPath(kLauncher[1], kLauncher[2], vx, vy, {steps = 120, radius = kBallRadius})
        for index = 1, #points, 3 do
            graphics2d.drawCircle(points[index].x, points[index].y, 4, '#CCFFD54F', {layer = 4})
        end
    end
end

return Breakables
