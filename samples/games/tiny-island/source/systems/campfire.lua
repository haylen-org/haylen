-- The campfire burns wood as fuel and keeps a safe circle around it, a ring on the ground and a wall that only enemies bump into.
local audio = require('haylen.audio')
local graphics2d = require('haylen.graphics2d')

local config = require('config')
local flame = require('entities.flame')
local sound = require('systems.sound')

local campfire = {}
campfire.__index = campfire

local fire = config.fire
local segments = 40

local function radiusFor(fuel)
    return fire.minRadius + (fire.maxRadius - fire.minRadius) * fuel / fire.maxFuel
end

function campfire.new(world, x, y)
    local self = setmetatable({world = world, x = x, y = y, fuel = fire.startFuel}, campfire)
    self.radius = radiusFor(self.fuel)
    self.flame = flame.new(x, y)
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

-- Burns fuel for one frame and returns `true` when the circle grew, so the game can push enemies that are now inside it.
function campfire:update(dt, night)
    if self:lit() then
        self.fuel = math.max(0, self.fuel - (night and fire.nightBurn or fire.dayBurn) * dt)
    end
    self.radius = self:lit() and radiusFor(self.fuel) or 0
    self.flame:update(dt, self:lit())

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

-- Adds up to `count` pieces of wood and returns how many the fire took.
function campfire:feed(count)
    local room = math.ceil((fire.maxFuel - self.fuel) / fire.woodFuel)
    local taken = math.min(count, room)
    if taken > 0 then
        self.fuel = math.min(fire.maxFuel, self.fuel + taken * fire.woodFuel)
        self.radius = radiusFor(self.fuel)
    end
    return taken
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
    self.flame:draw()
end

function campfire:destroy()
    sound.stop(self.crackle)
end

-- Lights the island around the fire with the color the day and night cycle leaves room for.
function campfire:light(cycle)
    if self:lit() then
        self.flame:light(self.radius * 1.7 + 160, cycle:fill(flame.glow))
    end
end

return campfire
