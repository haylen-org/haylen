-- Instances of a class that reloads keep their identity and their fields, and every one of them calls the new methods of the class from the next frame on. The walkers run on a meadow between bushes and rocks, turning away from each other and from them, each over a ring in the color that the class returns, as large as its radius says, and the panel shows what the class returns now.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local DevelopmentTest = require('categories.development.development-test')
local Meadow = require('categories.animation.meadow')
local Walker = require('categories.development.edit.walker')

local Classes = haylen.class('Classes', DevelopmentTest)

Classes.code = [[
local Walker = haylen.class('Walker')  -- The class of edit/walker.lua keeps its identity.
function Walker:speed()
    return 140  -- Every walker takes a new value where it stands.
end
self.walkers[index] = Walker(x, y, heading)
walker:step(dt, self.area)]]

Classes.files = {'source/categories/development/edit/walker.lua'}
Classes.steps = {'Change what "Walker:speed", "Walker:color" or "Walker:radius" returns.', 'Save the file.', 'Every walker takes the new method where it stands, because the class and its instances keep their identity.'}
Classes.count = 12
-- The frames of the walker sheet, its run and the share of a frame from its top to the feet.
Classes.cell = 256
Classes.run = {9, 16}
Classes.feet = 0.92
-- The bushes and rocks of the tiles of the Tiled tests, the share of a prop from its top to its base, the size it draws at, how far apart they stand and the reach of their base.
Classes.props = {{266, 266, 128, 128}, {398, 266, 128, 128}}
Classes.propBase = 0.82
Classes.propSize = 112
Classes.propSpacing = 300
Classes.propReach = 40
-- Two walkers keep this many radii apart, measured on the ground, where a step toward the viewer counts this many times a step to the side.
Classes.spacing = 2
Classes.depth = 1.6

function Classes:init(entry)
    Classes.super.init(self, entry)
    self.walkers = {}
    self.props = {}
end

function Classes:enter()
    Classes.super.enter(self)
    self.sheet = assets.texture('sprites/sheets/walker.png', {filter = 'linear', mipmaps = true})
    self.terrain = assets.texture('tiled/tiles/terrain.png', {filter = 'linear', mipmaps = true})
end

-- What every walker asks the class, which changes for all of them once the file is saved.
function Classes:controls()
    return {
        ui.sectionTitle{text = 'What the class returns now'},
        ui.card{gap = 8,
            ui.label{id = 'speed', text = ''},
            ui.label{id = 'radius', text = ''},
            ui.label{id = 'color', text = ''},
        },
    }
end

function Classes:resize(area)
    self.meadow = Meadow(area, 96, nil, 0)
    self:scatterProps(area)
    if #self.walkers > 0 then
        return
    end
    for index = 1, Classes.count do
        local walker = Walker(area.width * index / (Classes.count + 1), area.height * (0.35 + 0.4 * (index % 3) / 2), index * 0.9)
        walker.phase = index * 0.37
        self.walkers[index] = walker
    end
end

-- Bushes and rocks stand evenly apart, each whole inside the meadow.
function Classes:scatterProps(area)
    local margin = Classes.propSize * 0.6
    local points = m.poissonDisk({area = m.rect(margin, margin * 1.4, area.width - margin * 2, area.height - margin * 2), minimumDistance = Classes.propSpacing, seed = 7})
    self.props = {}
    for index, point in ipairs(points) do
        self.props[index] = {x = point.x, y = point.y, source = Classes.props[index % 2 + 1]}
    end
end

function Classes:update(dt)
    Classes.super.update(self, dt)
    if self.area then
        for _, walker in ipairs(self.walkers) do
            walker:step(dt, self.area)
        end
        self:separate()
    end
    local walker = self.walkers[1]
    if walker then
        self:showClass(walker:speed(), walker:radius(), walker:color())
    end
end

-- Turns walkers that come too close away from each other and from the props, so the crowd spreads over the meadow instead of piling up.
function Classes:separate()
    local walkers = self.walkers
    for first = 1, #walkers do
        local a = walkers[first]
        for second = first + 1, #walkers do
            local b = walkers[second]
            if Classes.near(a, b, (a:radius() + b:radius()) * Classes.spacing) then
                a.heading = Classes.away(b, a)
                b.heading = Classes.away(a, b)
            end
        end
        for _, prop in ipairs(self.props) do
            if Classes.near(a, prop, a:radius() * 1.4 + Classes.propReach) then
                a.heading = Classes.away(prop, a)
            end
        end
    end
end

function Classes.near(a, b, reach)
    local dx, dy = b.x - a.x, (b.y - a.y) * Classes.depth
    return dx * dx + dy * dy < reach * reach
end

-- The heading that leads `walker` away from `from`.
function Classes.away(from, walker)
    return math.atan((walker.y - from.y) * Classes.depth, walker.x - from.x)
end

function Classes:showClass(speed, radius, color)
    local text = string.format('%s %s %s', speed, radius, color)
    if text == self.classText then
        return
    end
    self.classText = text
    self:set('speed', {text = string.format('Speed %s points per second', speed)})
    self:set('radius', {text = string.format('Radius %s points', radius)})
    self:set('color', {text = string.format('Ring color "%s"', color), style = {colors = {text = color}}})
end

-- The walkers and the props draw in layers by the line of their feet or their base, so the nearer one stands in front, and each walker has its ring under its feet.
function Classes:draw(area)
    self.meadow:draw()
    for _, prop in ipairs(self.props) do
        graphics2d.draw(self.terrain, prop.x, prop.y, {source = prop.source, width = Classes.propSize, height = Classes.propSize, pivotY = Classes.propBase, layer = math.floor(prop.y)})
    end
    local first, last = Classes.run[1], Classes.run[2]
    for _, walker in ipairs(self.walkers) do
        local radius = walker:radius()
        local frame = first + math.floor((haylen.elapsed() + walker.phase) * 14) % (last - first + 1)
        local size = radius * 6
        local layer = math.floor(walker.y)
        graphics2d.drawShape({walker.x - radius * 1.4, walker.y - radius * 0.45, radius * 2.8, radius * 0.9}, {kind = 'ellipse', color = '#40000000', borderWidth = 4, borderColor = walker:color(), layer = layer})
        graphics2d.draw(self.sheet, walker.x, walker.y, {source = {(frame - 1) % 8 * Classes.cell, (frame - 1) // 8 * Classes.cell, Classes.cell, Classes.cell}, width = size, height = size, pivotY = Classes.feet, flipHorizontal = math.cos(walker.heading) < 0, layer = layer})
    end
end

return Classes
