-- The campfire burns wood as fuel and keeps a safe circle around it, a ring on the ground and a wall that only enemies bump into.
local assets = require('haylen.assets')
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local m = require('haylen.math')
local particles2d = require('haylen.particles2d')

local art = require('systems.art')
local config = require('config')
local sound = require('systems.sound')

local campfire = {}
campfire.__index = campfire

local fire = config.fire
local segments = 40
local glow = m.color('#FFFFB870')

local function radiusFor(fuel)
    return fire.minRadius + (fire.maxRadius - fire.minRadius) * fuel / fire.maxFuel
end

function campfire.new(world, x, y)
    local self = setmetatable({world = world, x = x, y = y, fuel = fire.startFuel, time = 0}, campfire)
    self.radius = radiusFor(self.fuel)
    -- The Tiny Swords fire strips are single puffs, so the flame is a stream of overlapping puffs over a glow of particles.
    self.puff = art.strip('effects/fire_03.png', 64, {fps = 18, loop = false})
    self.puffs = {}
    self.puffTime = 0
    self.logs = art.texture('terrain/resources/wood/wood_resource/wood_resource.png')
    self.flames = particles2d.newEmitter(assets.load('effects/flames.particles'))
    self.embers = particles2d.newEmitter(assets.load('effects/embers.particles'))
    for _, emitter in ipairs({self.flames, self.embers}) do
        emitter.x = x
        emitter.y = y - 10
    end
    self.crackle = sound.loop('ambient/fire_loop.ogg', 0.7)
    self.crackling = true
    self:buildBarrier()
    return self
end

function campfire:lit()
    return self.fuel > 0
end

-- The barrier is a closed chain at the current radius, rebuilt whenever the circle changes enough to matter.
function campfire:buildBarrier()
    if self.barrier then
        self.barrier:destroy()
        self.barrier = nil
    end
    self.barrierRadius = self.radius
    if not self:lit() then
        return
    end
    local points = {}
    for index = 0, segments - 1 do
        local angle = index / segments * math.pi * 2
        points[#points + 1] = {math.cos(angle) * self.radius, math.sin(angle) * self.radius}
    end
    self.barrier = self.world:createBody({type = 'static', x = self.x, y = self.y})
    self.barrier:addChain(points, true, {category = config.category.barrier, mask = config.category.enemy})
end

-- Burns fuel for one frame and returns true when the circle grew, so the game can push enemies that are now inside it.
function campfire:update(dt, night)
    self.time = self.time + dt
    if self:lit() then
        self.fuel = math.max(0, self.fuel - (night and fire.nightBurn or fire.dayBurn) * dt)
    end
    self.radius = self:lit() and radiusFor(self.fuel) or 0
    for _, emitter in ipairs({self.flames, self.embers}) do
        emitter.emitting = self:lit()
        emitter:update(dt)
    end
    self:updatePuffs(dt)

    -- The crackle follows the flames, so it goes quiet while the fire is out and comes back when wood relights it.
    if self:lit() ~= self.crackling then
        self.crackling = self:lit()
        if self.crackling then
            audio.resume(self.crackle)
        else
            audio.pause(self.crackle)
        end
    end

    local grew = self.radius > self.barrierRadius + 8
    if grew or self.radius < self.barrierRadius - 8 or (not self:lit() and self.barrier) then
        self:buildBarrier()
    end
    return grew
end

-- Adds up to count pieces of wood and returns how many the fire took.
function campfire:feed(count)
    local room = math.ceil((fire.maxFuel - self.fuel) / fire.woodFuel)
    local taken = math.min(count, room)
    if taken > 0 then
        self.fuel = math.min(fire.maxFuel, self.fuel + taken * fire.woodFuel)
        self.radius = radiusFor(self.fuel)
    end
    return taken
end

function campfire:updatePuffs(dt)
    for index = #self.puffs, 1, -1 do
        local puff = self.puffs[index]
        puff.time = puff.time + dt
        if puff.time >= self.puff.duration then
            table.remove(self.puffs, index)
        end
    end
    self.puffTime = self.puffTime - dt
    if self:lit() and self.puffTime <= 0 then
        local size = 0.6 + self.fuel / fire.maxFuel
        self.puffTime = 0.12
        self.puffs[#self.puffs + 1] = {time = 0, x = (math.random() - 0.5) * 30 * size, y = (math.random() - 0.5) * 10, scale = size * (0.8 + math.random() * 0.4)}
    end
end

function campfire:newDay()
    self.fuel = math.max(0, self.fuel - fire.dailyLoss)
end

function campfire:contains(x, y, margin)
    local dx, dy = x - self.x, y - self.y
    return self:lit() and dx * dx + dy * dy < (self.radius + (margin or 0)) ^ 2
end

function campfire:draw()
    if self:lit() then
        graphics2d.drawCircle(self.x, self.y, self.radius, '#26FFC266', {layer = config.layer.ring})
        graphics2d.drawRing(self.x, self.y, self.radius, 5, '#B3FFD27A', {layer = config.layer.ring})
    end
    for index = -1, 1 do
        graphics2d.draw(self.logs, self.x + index * 18, self.y + 10 - math.abs(index) * 4, {pivotX = 0.5, pivotY = 0.5, scaleX = 0.8, scaleY = 0.8, rotation = index * 0.5, layer = config.layer.entities, depth = self.y - 1})
    end
    for _, puff in ipairs(self.puffs) do
        local source = self.puff:frame(self.puff:frameAt(puff.time))
        graphics2d.draw(self.puff.texture, self.x + puff.x, self.y + puff.y, {source = source, pivotX = 0.5, pivotY = 0.85, scaleX = puff.scale, scaleY = puff.scale, layer = config.layer.entities, depth = self.y + 1})
    end
    if self:lit() then
        self.flames:draw()
        self.embers:draw()
    end
end

function campfire:destroy()
    sound.stop(self.crackle)
end

-- Lights the island around the fire with the color the day and night cycle leaves room for.
function campfire:light(cycle)
    if self:lit() then
        local intensity = lighting2d.flicker(self.time, {speed = 7, amount = 0.12})
        graphics2d.drawLight({x = self.x, y = self.y - 20, radius = self.radius * 1.7 + 160, color = cycle:fill(glow), intensity = intensity})
    end
end

return campfire
