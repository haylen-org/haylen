-- A tree that sways in the wind, shakes when chopped, falls into a stump and grows back two days later.
local graphics2d = require('haylen.graphics2d')

local art = require('systems.art')
local config = require('config')

local tree = {}
tree.__index = tree

-- The trees are painted large, and the island shows them at this scale.
local scale = 0.62

local variants = {
    {frame = 'tree_oak', stump = 'stump_oak'},
    {frame = 'tree_tall', stump = 'stump_oak'},
    {frame = 'tree_palm', stump = 'stump_palm'},
    {frame = 'tree_fruit', stump = 'stump_oak'},
}

function tree.new(world, x, y, variant)
    local spec = variants[variant]
    local self = setmetatable({x = x, y = y, time = math.random() * 6, shake = 0}, tree)
    self.atlas = art.props()
    self.source = self.atlas:source(spec.frame)
    self.stumpSource = self.atlas:source(spec.stump)
    self.shadow = art.effects()
    self.shadowSource = self.shadow:source('shadow')
    self.body = world:createBody({type = 'static', x = x, y = y})
    self.body:addCircle(20, {category = config.category.tree, mask = config.category.player | config.category.enemy | config.category.sheep})
    self.health = config.trees.health
    return self
end

function tree:standing()
    return self.fellDay == nil
end

-- Takes one hit of the given strength and returns `true` when the tree falls.
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
    local layer = config.layer.entities
    graphics2d.draw(self.shadow.texture, self.x + 4, self.y + 3, {source = self.shadowSource, width = 100, height = 32, color = '#78FFFFFF', layer = layer, depth = self.y - 1})
    if not self:standing() then
        graphics2d.draw(self.atlas.texture, self.x, self.y + 7, {source = self.stumpSource, pivotX = 0.5, pivotY = 1, scaleX = scale, scaleY = scale, layer = layer, depth = self.y})
        return
    end

    -- The crown sways around the foot of the trunk, and a chop shakes it hard for a moment.
    local sway = math.sin(self.time * 1.3) * 0.018 + math.sin(self.time * 31) * 0.05 * self.shake
    graphics2d.draw(self.atlas.texture, self.x, self.y + 8, {source = self.source, pivotX = 0.5, pivotY = 1, scaleX = scale, scaleY = scale, rotation = sway, layer = layer, depth = self.y})
end

return tree
