-- A tree that sways in the wind, shakes when chopped, falls into a stump and grows back two days later.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local config = require('config')

local tree = {}
tree.__index = tree

local variants = {
    {file = 'tree1', height = 256, pivotY = 0.94},
    {file = 'tree2', height = 256, pivotY = 0.97},
    {file = 'tree3', height = 192, pivotY = 0.885},
    {file = 'tree4', height = 192, pivotY = 0.875},
}

function tree.new(world, x, y, variant)
    local spec = variants[variant]
    local self = setmetatable({x = x, y = y, time = math.random() * 2, shake = 0}, tree)
    self.clip = art.strip('terrain/resources/wood/trees/' .. spec.file .. '.png', 192, {height = spec.height, fps = 8})
    self.stumpTexture = art.texture('terrain/resources/wood/trees/stump_' .. variant .. '.png')
    self.sprite = graphics2d.newSprite(self.clip.texture, {x = x, y = y, pivotX = 0.5, pivotY = spec.pivotY, layer = config.layer.entities, depth = y})
    self.body = world:createBody({type = 'static', x = x, y = y})
    self.body:addCircle(24, {category = config.category.tree, mask = config.category.player | config.category.enemy})
    self.health = config.trees.health
    return self
end

function tree:standing()
    return self.fellDay == nil
end

-- Takes one hit of the given strength and returns true when the tree falls.
function tree:chop(strength)
    self.health = self.health - strength
    self.shake = 1
    return self.health <= 0
end

function tree:fell(day)
    self.fellDay = day
end

function tree:regrow(day)
    if self.fellDay and day >= self.fellDay + config.trees.regrowDays then
        self.fellDay = nil
        self.health = config.trees.health
    end
end

function tree:update(dt)
    self.time = self.time + dt
    self.shake = math.max(0, self.shake - dt * 4)
end

function tree:draw()
    if self:standing() then
        self.sprite.source = self.clip:frame(self.clip:frameAt(self.time))
        self.sprite.x = self.x + math.sin(self.time * 60) * 6 * self.shake
        self.sprite:draw()
    else
        graphics2d.draw(self.stumpTexture, self.x, self.y, {pivotX = 0.5, pivotY = 0.93, layer = config.layer.entities, depth = self.y})
    end
end

return tree
