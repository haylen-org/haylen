-- The island behind the menus at dusk: the four survivors rest around the fire with a few sheep grazing nearby while the camera drifts, or frames the one being chosen.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local classes = require('data.classes')
local config = require('config')
local flame = require('entities.flame')
local island = require('systems.island')
local tree = require('entities.tree')

local backdrop = {}
backdrop.__index = backdrop

local restRadius = 200

-- Where the sheep graze, relative to the fire.
local grazing = {{-430, 150, 1}, {-520, 60, -1}, {460, 170, -1}}

local function figure(name, x, y, facing)
    local animator, character = art.newAnimator(name)
    local sprite = graphics2d.newSprite(character.atlas.texture, {x = x, y = y, scaleX = character.scale, scaleY = character.scale, flipHorizontal = facing < 0, layer = config.layer.entities, depth = y})
    return {x = x, y = y, animator = animator, sprite = sprite}
end

function backdrop.new()
    local self = setmetatable({time = 0, units = {}, figures = {}, trees = {}}, backdrop)
    self.island = island.new()
    self.fire = flame.new(self.island.fire.x, self.island.fire.y)
    for _, spot in ipairs(self.island:treeSpots()) do
        self.trees[#self.trees + 1] = tree.new(self.island.world, spot.x, spot.y, spot.variant)
    end

    -- The survivors sit on an arc behind the fire and face it.
    for index, class in ipairs(classes) do
        local angle = math.pi * (1.1 + 0.8 * (index - 1) / (#classes - 1))
        local x = self.fire.x + math.cos(angle) * restRadius * 1.3
        local y = self.fire.y + math.sin(angle) * restRadius + 40
        local unit = figure(class.id, x, y, x > self.fire.x and -1 or 1)
        self.units[class.id] = unit
        self.figures[#self.figures + 1] = unit
    end
    for _, spot in ipairs(grazing) do
        self.figures[#self.figures + 1] = figure('sheep', self.fire.x + spot[1], self.fire.y + spot[2], spot[3])
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

-- Plays the attack of a survivor once, which returns to its idle loop afterwards.
function backdrop:cheer(id)
    self.units[id].animator:play('attack', true)
end

function backdrop:update(dt)
    self.time = self.time + dt
    self.island:update(dt)
    self.fire:update(dt, true)
    for _, standing in ipairs(self.trees) do
        standing:update(dt)
    end
    for _, unit in ipairs(self.figures) do
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
    graphics2d.beginWorld(self.camera, {sort = 'depth', ambientLight = '#FF8C84B4', postProcess = {vignetteStrength = 0.35}})
    self.island:drawGround(self.camera)
    self.fire:draw()
    for _, standing in ipairs(self.trees) do
        standing:draw()
    end
    for _, unit in ipairs(self.figures) do
        unit.animator:apply(unit.sprite)
        unit.sprite:draw()
    end
    self.island:drawClouds(self.camera)
    self.fire:light(620, flame.glow)
end

return backdrop
