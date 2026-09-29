-- Sprite batches: a field of gems kept in one SpriteBatch whose sprites the test adds, changes and removes, drawn as one batch or one draw at a time for comparison.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Batches = haylen.class('Batches', sample.Test)

local kIcons = {'gem_red.png', 'gem_green.png', 'gem_blue.png', 'star.png', 'heart.png'}
local kSpacing = 40
local kCode = [[
local batch = graphics2d.newSpriteBatch(gems.texture)  batch:reserve(4000)
local index = batch:add({x = 100, y = 100, width = 28, height = 28, source = gems:source('star.png')})
batch:set(index, {rotation = 0.5, color = '#FFFFD166'})  batch:remove(index)  batch:get(index)  batch:size()
batch:draw()  -- one batch for every sprite]]

function Batches:enter()
    self.gems = sample.atlas('atlases/gems.json')
    self.sources = {}
    for index, name in ipairs(kIcons) do
        self.sources[index] = self.gems:source(name)
    end
    self.batch = graphics2d.newSpriteBatch(self.gems.texture)
    self.batch:reserve(4000)
    self.oneByOne = false
    self.time = 0
    self:frame({
        hint = 'Click or tap a gem to remove it, or empty space to add one. The wave turns one row per moment with batch:set.',
        code = kCode,
        controls = {
            ui.button{id = 'add', text = 'Add 500 gems', variant = 'primary', onClick = function() self:scatter(500) end},
            ui.button{id = 'refill', text = 'Refill the grid', onClick = function() self:resize(self.area) end},
            ui.button{id = 'clear', text = 'Clear', variant = 'destructive', onClick = function() self.batch:clear() end},
            ui.toggle{id = 'single', text = 'One draw per gem', onChange = function(event) self.oneByOne = event.checked end},
        },
        focus = 'add',
    })
end

function Batches:gem(x, y)
    return {x = x, y = y, width = 28, height = 28, source = self.sources[math.random(#self.sources)]}
end

-- Fills the stage with a grid of gems.
function Batches:resize(area)
    self.columns = math.floor(area.width / kSpacing)
    self.batch:clear()
    for row = 0, math.floor(area.height / kSpacing) - 1 do
        for column = 0, self.columns - 1 do
            self.batch:add(self:gem((column + 0.5) * kSpacing, (row + 0.5) * kSpacing))
        end
    end
end

function Batches:scatter(count)
    for _ = 1, count do
        self.batch:add(self:gem(math.random() * self.area.width, math.random() * self.area.height))
    end
end

-- Removes the gem under the point, or adds one there when no gem is close.
function Batches:touch(x, y)
    for index = self.batch:size(), 1, -1 do
        local gem = self.batch:get(index)
        if math.abs(gem.x - x) < 16 and math.abs(gem.y - y) < 16 then
            self.batch:remove(index)
            return
        end
    end
    self.batch:add(self:gem(x, y))
end

function Batches:update(dt)
    Batches.super.update(self, dt)
    self.time = self.time + dt
    local x, y, pressed = self:pointer()
    if pressed and self.area and self.area:contains({x, y}) then
        self:touch(x, y)
    end

    -- A wave turns the gems of one grid row at a time.
    local size = self.batch:size()
    if self.columns and size > 0 then
        local rows = math.max(1, math.floor(size / self.columns))
        local row = math.floor(self.time * 6) % rows
        for index = row * self.columns + 1, math.min(size, (row + 1) * self.columns) do
            self.batch:set(index, {rotation = self.time * 4})
        end
    end
    local stats = graphics2d.stats()
    self:status(string.format('%d gems   %s   %d draw calls   %d sprites submitted', size, self.oneByOne and 'one draw each' or 'one batch', stats.drawCalls, stats.sprites))
end

function Batches:draw(area)
    if not self.oneByOne then
        self.batch:draw()
        return
    end
    local texture = self.gems.texture
    for index = 1, self.batch:size() do
        local gem = self.batch:get(index)
        graphics2d.draw(texture, gem.x, gem.y, {source = gem.source, width = gem.width, height = gem.height, rotation = gem.rotation})
    end
end

return Batches
