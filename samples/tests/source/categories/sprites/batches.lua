-- Sprite batches: a field of gems kept in one `SpriteBatch` whose sprites the test adds, changes and removes, drawn as one batch or one draw at a time for comparison. A wave runs down the field and turns each gem it passes once around.
local debugging = require('haylen.debug')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local localization = require('haylen.localization')
local m = require('haylen.math')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local Batches = haylen.class('Batches', SpriteTest)

Batches.icons = {'gem-red.png', 'gem-green.png', 'gem-blue.png', 'star.png', 'heart.png'}
Batches.spacing = 42
-- The longer side of every gem, which keeps the shape of its frame.
Batches.size = 32
-- The wave moves down this many units per second and turns the gems over a band this tall.
Batches.waveSpeed = 320
Batches.waveBand = 150

Batches.code = [[
local batch = graphics2d.newSpriteBatch(gems.texture)
batch:reserve(4000)
local index = batch:add({x = 100, y = 100, width = 26, height = 32, source = gems:source('gem-red.png')})
batch:set(index, {rotation = 0.5})
batch:remove(index)
batch:draw()  -- One call from Lua for every gem of the batch.]]

function Batches:enter()
    self.gems = SpriteTest.atlas('atlases/gems.json')
    self.kinds = {}
    for index, name in ipairs(Batches.icons) do
        local source = self.gems:source(name)
        local scale = Batches.size / math.max(source.width, source.height)
        self.kinds[index] = {source = source, width = source.width * scale, height = source.height * scale}
    end
    self.random = m.random(4)
    -- The gems lie on a tray of dark velvet that glows a little in the middle.
    local velvet = graphics.newImage(128, 128)
    velvet:fillGradient({kind = 'radial', radius = 0.75, stops = {{0, '#FF2E3558'}, {0.7, '#FF1C2036'}, {1, '#FF131626'}}})
    self.velvet = graphics.newTexture(velvet, {filter = 'linear'})
    self.batch = graphics2d.newSpriteBatch(self.gems.texture)
    self.batch:reserve(4000)
    -- The y of every gem by its index in the batch, so the wave finds the gems it passes without reading the batch back.
    self.rows = {}
    self.turned = {rotation = 0}
    self.oneByOne = false
    self.wave = 0
    self:frame{
        code = Batches.code,
        hint = 'Click or tap a gem to remove it, or empty space to add one. The wave turns the gems it passes with "batch:set".',
        controls = {
            ui.button{id = 'add', text = 'Scatter 500 gems', variant = 'primary', onClick = function() self:scatter(500) end},
            ui.button{id = 'refill', text = 'Refill the grid', onClick = function() self:fill(self.area) end},
            ui.button{id = 'clear', text = 'Remove every gem', variant = 'destructive', onClick = function() self:clear() end},
            ui.toggle{id = 'single', text = 'Draw each gem on its own', onChange = function(event) self.oneByOne = event.checked end},
        },
        focus = 'add',
    }
end

function Batches:resize(area)
    self:fill(area)
end

-- Adds a gem of a random kind with its pivot at a point.
function Batches:add(x, y)
    local kind = self.kinds[self.random:integer(1, #self.kinds)]
    self.batch:add({x = x, y = y, width = kind.width, height = kind.height, source = kind.source})
    self.rows[#self.rows + 1] = y
end

function Batches:clear()
    self.batch:clear()
    self.rows = {}
end

-- Fills the stage with a grid of gems centered in it.
function Batches:fill(area)
    self:clear()
    local spacing = Batches.spacing
    local columns, rows = math.floor(area.width / spacing), math.floor(area.height / spacing)
    local left, top = (area.width - columns * spacing) / 2, (area.height - rows * spacing) / 2
    for row = 0, rows - 1 do
        for column = 0, columns - 1 do
            self:add(left + (column + 0.5) * spacing, top + (row + 0.5) * spacing)
        end
    end
end

function Batches:scatter(count)
    local random, margin = self.random, Batches.size / 2
    for _ = 1, count do
        self:add(random:range(margin, self.area.width - margin), random:range(margin, self.area.height - margin))
    end
end

-- Removes the gem under the point, or adds one there when no gem is close.
function Batches:touch(x, y)
    local reach = Batches.size / 2
    for index = self.batch:size(), 1, -1 do
        local gem = self.batch:get(index)
        if math.abs(gem.x - x) < reach and math.abs(gem.y - y) < reach then
            self.batch:remove(index)
            table.remove(self.rows, index)
            return
        end
    end
    self:add(x, y)
end

-- Turns the gems inside the band above the wave once around, and sets the gems the band left this frame back to no rotation.
function Batches:turn(dt)
    local band, batch, fields = Batches.waveBand, self.batch, self.turned
    local before, wave = self.wave, self.wave + Batches.waveSpeed * dt
    for index, y in ipairs(self.rows) do
        if y <= wave and y > before - band then
            local progress = (wave - y) / band
            fields.rotation = progress < 1 and m.tau * m.ease('cubicInOut', progress) or 0
            batch:set(index, fields)
        end
    end
    -- The wave starts over from the top once the band has left every gem.
    self.wave = wave > self.area.height + band and 0 or wave
end

function Batches:update(dt)
    Batches.super.update(self, dt)
    if not self.area then
        return
    end
    local x, y, pressed = self:pointer()
    if pressed then
        self:touch(x, y)
    end
    self:turn(dt)

    local count, frame = self.batch:size(), debugging.frame()
    local calls = self.oneByOne and localization.formatNumber(count) .. ' draws from Lua' or 'one draw from Lua'
    self:status(string.format('%s gems with %s. The frame takes %.2f ms and %d draw calls, the interface included.', localization.formatNumber(count), calls, frame.milliseconds, graphics2d.stats().drawCalls))
end

function Batches:draw(area)
    graphics2d.draw(self.velvet, 0, 0, {width = area.width, height = area.height, pivotX = 0, pivotY = 0, layer = -1})
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
