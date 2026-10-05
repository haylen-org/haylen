-- Explosive barrels that blow up with `physics2d.explode` when they are clicked or hit hard enough, lighting the fuses of the barrels their blast reaches in a chain reaction that scatters the crates.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local physics2d = require('haylen.physics2d')
local ui = require('haylen.ui')

local parts = require('categories.physics.parts')
local PhysicsTest = require('categories.physics.physics-test')

local Barrels = haylen.class('Barrels', PhysicsTest)

local kRadius = 300
local kHitSpeed = 600
local kBlastLife = 0.6
local kBarrelColor = '#FFE53935'

function Barrels:enter()
    self:frame{
        hint = 'Tap or click a barrel to set it off. A blast lights the barrels it reaches, and a barrel hit hard enough by flying debris goes off too. R or the X button rebuilds.',
        controls = {
            ui.button{id = 'fire', text = 'Set off a barrel', onClick = function() self:lightFirst() end},
            ui.checkbox{id = 'chain', text = 'Chain reactions', checked = true, onChange = function(event) self.chaining = event.checked end},
            ui.label{text = 'Blast impulse', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'impulse', value = 700, min = 200, max = 1600, step = 50, showValue = true, decimals = 0, onChange = function(event) self.impulse = event.value end},
            ui.button{id = 'reset', text = 'Rebuild', onClick = function() self:build() end},
        },
        focus = 'fire',
    }
    self.random = m.random(55)
    self.chaining, self.impulse = true, 700
    self:build()
end

function Barrels:build()
    self.world = physics2d.newWorld()
    self.barrels, self.crates, self.blasts = {}, {}, {}
    self.exploded, self.longest, self.depth = 0, 0, 0

    local ground = self.world:createBody({type = 'static'})
    parts.box(ground, 1600, 40, {offsetY = 410})
    parts.box(ground, 400, 20, {offsetX = -300, offsetY = 160})
    parts.box(ground, 400, 20, {offsetX = 350, offsetY = 40})
    self.statics = {ground}

    for _, x in ipairs({-600, -300, 0, 300, 600}) do
        self:barrel(x, 355)
    end
    self:barrel(-420, 115)
    self:barrel(-180, 115)
    self:barrel(350, -5)
    for _, stack in ipairs({{-450, 390, 3}, {-150, 390, 2}, {150, 390, 3}, {450, 390, 2}, {-300, 150, 2}, {250, 30, 2}, {450, 30, 1}}) do
        for level = 1, stack[3] do
            local crate = self.world:createBody({x = stack[1], y = stack[2] - 30 - (level - 1) * 61})
            parts.box(crate, 60, 60)
            parts.paint(crate, '#FFBC8F5A')
            self.crates[#self.crates + 1] = crate
        end
    end

    self.world.onHit = function(a, b, contact)
        if contact.speed > kHitSpeed then
            for _, body in ipairs({a, b}) do
                if body.data.barrel then
                    self:light(body, self.depth + 1, 0.05)
                end
            end
        end
    end
end

function Barrels:barrel(x, y)
    local barrel = self.world:createBody({x = x, y = y})
    parts.box(barrel, 50, 70, {density = 1.5})
    parts.paint(barrel, kBarrelColor)
    barrel.data.barrel = true
    self.barrels[#self.barrels + 1] = barrel
end

function Barrels:light(barrel, depth, fuse)
    if not barrel.data.fuse then
        barrel.data.fuse, barrel.data.depth = fuse, depth
    end
end

function Barrels:lightFirst()
    if #self.barrels > 0 then
        self:light(self.barrels[1], 1, 0)
    end
end

-- The barrel goes before its blast, so the blast only pushes what is around it, and every barrel the blast reaches gets a short fuse.
function Barrels:explode(index)
    local barrel = table.remove(self.barrels, index)
    local x, y, depth = barrel.x, barrel.y, barrel.data.depth
    barrel:destroy()
    local hits = physics2d.explode(self.world, {x = x, y = y, radius = kRadius, impulse = self.impulse, falloff = 'linear'})
    self.blasts[#self.blasts + 1] = {x = x, y = y, life = kBlastLife}
    self.exploded, self.depth = self.exploded + 1, depth
    self.longest = math.max(self.longest, depth)
    if self.chaining then
        for _, hit in ipairs(hits) do
            if hit.body.data.barrel then
                self:light(hit.body, depth + 1, self.random:range(0.12, 0.25))
            end
        end
    end
end

function Barrels:update(dt)
    Barrels.super.update(self, dt)
    if input.pressed('reset') then
        self:build()
    end
    if self.pointer.pressed then
        for _, shape in ipairs(self.world:pick(self.camera, self.pointer.x, self.pointer.y, {radius = 12})) do
            if shape.body.data.barrel then
                self:light(shape.body, 1, 0)
                break
            end
        end
    end
    for index = #self.blasts, 1, -1 do
        local blast = self.blasts[index]
        blast.life = blast.life - dt
        if blast.life <= 0 then
            table.remove(self.blasts, index)
        end
    end
    self:status(string.format('Barrels left %d, blasts %d, longest chain %d, bodies %d, step %.2f ms', #self.barrels, self.exploded, self.longest, self.world.bodyCount, self:stepTime()))
end

function Barrels:fixedUpdate(step)
    for index = #self.barrels, 1, -1 do
        local data = self.barrels[index].data
        if data.fuse then
            data.fuse = data.fuse - step
            if data.fuse <= 0 then
                self:explode(index)
            end
        end
    end
    self:simulate(self.world, step)
end

function Barrels:draw(area)
    parts.drawAll(self.statics)
    parts.drawAll(self.crates, {layer = 1})
    for _, barrel in ipairs(self.barrels) do
        local lit = barrel.data.fuse and math.floor(barrel.data.fuse * 20) % 2 == 0
        parts.paint(barrel, lit and '#FFFFEB3B' or kBarrelColor)
        parts.draw(barrel, {layer = 1})
        local cos, sin = math.cos(barrel.rotation), math.sin(barrel.rotation)
        for _, band in ipairs({-20, 20}) do
            local x, y = barrel.x - band * sin, barrel.y + band * cos
            graphics2d.drawLine(x - cos * 24, y - sin * 24, x + cos * 24, y + sin * 24, 5, '#AA3E2723', {layer = 2})
        end
    end
    for _, blast in ipairs(self.blasts) do
        local age = 1 - blast.life / kBlastLife
        graphics2d.drawCircle(blast.x, blast.y, kRadius * age, m.color(1, 0.6, 0.2, 0.4 * (1 - age)), {layer = 5, blend = 'additive'})
        graphics2d.drawRing(blast.x, blast.y, kRadius * age, 4, m.color(1, 0.85, 0.4, 1 - age), {layer = 5})
    end
end

return Barrels
