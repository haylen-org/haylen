-- Instances of a class that reloads keep their identity and their fields, and every one of them calls the new methods of the class from the next frame on.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local DevelopmentTest = require('categories.development.development-test')
local Walker = require('categories.development.edit.walker')

local Classes = haylen.class('Classes', DevelopmentTest)

Classes.file = 'source/categories/development/edit/walker.lua'
Classes.steps = {'Change what "Walker:speed", "Walker:color" or "Walker:radius" returns.', 'Save the file.', 'Every walker takes the new method where it stands, because the class and its instances keep their identity.'}
Classes.count = 12

function Classes:init(entry)
    Classes.super.init(self, entry)
    self.walkers = {}
end

function Classes:resize(area)
    if #self.walkers > 0 then
        return
    end
    for index = 1, Classes.count do
        self.walkers[index] = Walker(area.width * index / (Classes.count + 1), area.height / 2, index * 0.9)
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

function Classes:draw(area)
    for _, walker in ipairs(self.walkers) do
        graphics2d.drawCircle(walker.x, walker.y, walker:radius(), walker:color())
    end
end

return Classes
