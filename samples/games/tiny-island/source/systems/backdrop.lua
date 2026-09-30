-- The island behind the menus at dusk: the five survivors rest around the fire while the camera drifts, or frames the one being chosen.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local lighting2d = require('haylen.lighting2d')
local particles2d = require('haylen.particles2d')

local art = require('systems.art')
local classes = require('data.classes')
local config = require('config')
local island = require('systems.island')
local tree = require('entities.tree')

local backdrop = {}
backdrop.__index = backdrop

local restRadius = 190

function backdrop.new()
    local self = setmetatable({time = 0, units = {}, trees = {}}, backdrop)
    self.island = island.new()
    self.fire = self.island.fire
    self.logs = art.texture('terrain/resources/wood/wood_resource/wood_resource.png')
    self.flames = particles2d.newEmitter(assets.load('effects/flames.particles'))
    self.flames.x = self.fire.x
    self.flames.y = self.fire.y - 10
    for _, spot in ipairs(self.island:treeSpots()) do
        self.trees[#self.trees + 1] = tree.new(self.island.world, spot.x, spot.y, spot.variant)
    end

    -- The survivors sit on an arc behind the fire and face it.
    for index, class in ipairs(classes) do
        local angle = math.pi * (1.1 + 0.8 * (index - 1) / (#classes - 1))
        local x = self.fire.x + math.cos(angle) * restRadius * 1.3
        local y = self.fire.y + math.sin(angle) * restRadius + 40
        local animator, unit = art.newAnimator(class.unit, 'blue')
        local sprite = graphics2d.newSprite(unit.clips.idle.texture, {x = x, y = y, flipHorizontal = x > self.fire.x, layer = config.layer.entities, depth = y})
        self.units[class.id] = {x = x, y = y, animator = animator, sprite = sprite}
    end

    self.camera = graphics2d.newCamera()
    self.camera.limits = self.island.bounds
    self.camera.positionSmoothing = true
    self.camera.positionSmoothingSpeed = 3
    local start = self:drift()
    self.camera:snapTo(start[1], start[2])
    return self
end

function backdrop:drift()
    return {self.fire.x + math.cos(self.time * 0.07) * 420, self.fire.y + math.sin(self.time * 0.05) * 160 - 60}
end

-- Frames a survivor on the left of the screen, or lets the camera drift again when `id` is `nil`.
function backdrop:focus(id)
    self.focused = id and self.units[id]
end

-- Plays a one-shot clip of a survivor, which returns to its idle loop afterwards.
function backdrop:cheer(id)
    local unit = self.units[id]
    unit.animator:play('attack', true)
end

function backdrop:update(dt)
    self.time = self.time + dt
    self.island:update(dt)
    self.flames:update(dt)
    for _, standing in ipairs(self.trees) do
        standing:update(dt)
    end
    for _, unit in pairs(self.units) do
        unit.animator:update(dt)
        if unit.animator.finished then
            unit.animator:play('idle')
        end
    end

    local zoom = self.focused and 1.8 or 1
    local current = self.camera.zoom.x
    local eased = current + (zoom - current) * (1 - math.exp(-4 * dt))
    self.camera.zoom = {eased, eased}
    local target = self:drift()
    if self.focused then
        target = {self.focused.x + 250, self.focused.y - 70}
    end
    self.camera:follow(target[1], target[2], dt)
    self.camera:update(dt)
end

function backdrop:draw()
    graphics2d.beginWorld(self.camera, {sort = 'depth', ambientLight = '#FF6F6A9E', postProcess = {vignetteStrength = 0.35}})
    self.island:drawGround(self.camera)
    for index = -1, 1 do
        graphics2d.draw(self.logs, self.fire.x + index * 18, self.fire.y + 10 - math.abs(index) * 4, {pivotX = 0.5, pivotY = 0.5, scaleX = 0.8, scaleY = 0.8, rotation = index * 0.5, layer = config.layer.entities, depth = self.fire.y - 1})
    end
    self.flames:draw()
    for _, standing in ipairs(self.trees) do
        standing:draw()
    end
    for _, unit in pairs(self.units) do
        unit.animator:apply(unit.sprite)
        unit.sprite:draw()
    end
    self.island:drawClouds(self.camera)

    local intensity = lighting2d.flicker(self.time, {speed = 7, amount = 0.12})
    graphics2d.drawLight({x = self.fire.x, y = self.fire.y - 20, radius = 620, color = '#FFFFB870', intensity = intensity})
end

return backdrop
