-- Instances of a class that reloads keep their identity and their fields, and every one of them calls the new methods of the class from the next frame on. The walkers run on a meadow, each over a ring in the color of the class, as large as its radius says.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

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

Classes.file = 'source/categories/development/edit/walker.lua'
Classes.steps = {'Change what "Walker:speed", "Walker:color" or "Walker:radius" returns.', 'Save the file.', 'Every walker takes the new method where it stands, because the class and its instances keep their identity.'}
Classes.count = 12
-- The frames of the walker sheet, its run and the share of a frame from its top to the feet.
Classes.cell = 256
Classes.run = {9, 16}
Classes.feet = 0.92

function Classes:init(entry)
    Classes.super.init(self, entry)
    self.walkers = {}
    self.order = {}
end

function Classes:enter()
    Classes.super.enter(self)
    self.sheet = assets.texture('sprites/sheets/walker.png', {filter = 'linear', mipmaps = true})
end

function Classes:resize(area)
    self.meadow = Meadow(area, 96)
    if #self.walkers > 0 then
        return
    end
    for index = 1, Classes.count do
        local walker = Walker(area.width * index / (Classes.count + 1), area.height * (0.35 + 0.4 * (index % 3) / 2), index * 0.9)
        walker.phase = index * 0.37
        self.walkers[index] = walker
        self.order[index] = walker
    end
end

function Classes:update(dt)
    Classes.super.update(self, dt)
    if self.area then
        for _, walker in ipairs(self.walkers) do
            walker:step(dt, self.area)
        end
    end
end

-- The walkers draw from the top of the meadow down, so the nearer one stands in front, each with its ring under its feet.
function Classes:draw(area)
    self.meadow:draw()
    table.sort(self.order, function(a, b) return a.y < b.y end)
    local first, last = Classes.run[1], Classes.run[2]
    for _, walker in ipairs(self.order) do
        local radius = walker:radius()
        local frame = first + math.floor((haylen.elapsed() + walker.phase) * 14) % (last - first + 1)
        local size = radius * 6
        graphics2d.drawShape({walker.x - radius * 1.4, walker.y - radius * 0.45, radius * 2.8, radius * 0.9}, {kind = 'ellipse', color = '#40000000', borderWidth = 4, borderColor = walker:color()})
        graphics2d.draw(self.sheet, walker.x, walker.y, {source = {(frame - 1) % 8 * Classes.cell, (frame - 1) // 8 * Classes.cell, Classes.cell, Classes.cell}, width = size, height = size, pivotY = Classes.feet, flipHorizontal = math.cos(walker.heading) < 0})
    end
end

return Classes
