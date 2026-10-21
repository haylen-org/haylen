-- Animated crowd: thousands of fighters of one animated batch punch at their own speeds on a meadow, advanced in C++ by one call per frame and drawn in one draw call, each over a shadow of one sprite batch. A wave of single jabs sweeps across the crowd, and each fighter goes back to punching when its jab ends, which the batch reports in one list per update.
local animation2d = require('haylen.animation2d')
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')
local Meadow = require('categories.animation.meadow')

local Crowd = haylen.class('Crowd', AnimationTest)

Crowd.counts = {{id = '500', text = '500'}, {id = '2000', text = '2,000'}, {id = '8000', text = '8,000'}}
Crowd.scale = 0.18
-- The fighters stand this far apart, in pixels of their frames, so a punch never reaches the next fighter of the row.
Crowd.spacing = {480, 400}
Crowd.margin = 0.6
Crowd.waveSeconds = 1.6

Crowd.code = [[
local crowd = animation2d.newAnimatedBatch(atlas.texture, {punch = punch, jab = jab})
crowd:add({x = x, y = y, animation = 'punch', speed = 0.8, flipHorizontal = true})
crowd:play(index, 'jab', true)  -- A single jab that ends on its own.
crowd:on('complete', function(ended) ... end, {owner = self})  -- One list per update.
crowd:update(dt)  -- Every fighter in one call.
crowd:draw({layer = 1})  -- One draw call for the whole crowd.]]

function Crowd:enter()
    -- The crowd draws far smaller than the frames are painted, so its texture keeps smaller copies to sample.
    local atlas = assets.load('animation/fighter.json', 'atlas', AnimationTest.mipmapped)
    self.punch = atlas:animation('punch')
    self.jab = atlas:animation('punch')
    self.jab.loop = 'once'
    self.shadowTexture = Crowd.shadow()
    self.count = 500
    self.jabs = 0
    self:frame{
        code = Crowd.code,
        hint = 'Every fighter is a sprite of one batch with its own animation, time and speed.',
        controls = {
            ui.formField{label = 'Fighters', ui.stepper{id = 'count', items = Crowd.counts, selected = '500', onChange = function(event)
                self.count = tonumber(event.value)
                self:build()
            end}},
            ui.button{id = 'wave', text = 'Send a wave of jabs', variant = 'primary', onClick = function() self:wave() end},
        },
        focus = 'wave',
    }
end

-- A soft round shadow that every fighter stands on.
function Crowd.shadow()
    local image = graphics.newImage(64, 64)
    image:fillGradient({kind = 'radial', stops = {{0, '#70203018'}, {0.6, '#40203018'}, {1, '#00203018'}}})
    return graphics.newTexture(image, {filter = 'linear'})
end

-- Lays the crowd out in rows that keep the shape of the play area, with the last row centered under the others.
function Crowd:build()
    local count = self.count
    local dx, dy = Crowd.spacing[1] * Crowd.scale, Crowd.spacing[2] * Crowd.scale
    local columns = math.max(1, math.floor(math.sqrt(count * self.area.width / self.area.height * dy / dx) + 0.5))
    local rows = math.ceil(count / columns)
    self.columns, self.rows = columns, rows
    self.size = {columns * dx, rows * dy}

    self.crowd = animation2d.newAnimatedBatch(self.punch.texture, {punch = self.punch, jab = self.jab})
    self.crowd:reserve(count)
    self.shadows = graphics2d.newSpriteBatch(self.shadowTexture)
    self.shadows:reserve(count)
    self.columnOf = {}
    for index = 0, count - 1 do
        local row, column = index // columns, index % columns
        local inRow = row == rows - 1 and count - row * columns or columns
        local x = (column + 0.5 + (columns - inRow) / 2) * dx
        local y = (row + 0.9) * dy
        self.crowd:add({x = x, y = y, animation = 'punch', speed = 0.6 + (index * 37 % 9) * 0.1, scaleX = Crowd.scale, scaleY = Crowd.scale, flipHorizontal = row % 2 == 1})
        self.shadows:add({x = x, y = y, width = 230 * Crowd.scale, height = 70 * Crowd.scale})
        self.columnOf[index + 1] = x
    end
    self.order = {}
    for index = 1, count do
        self.order[index] = index
    end
    table.sort(self.order, function(a, b) return self.columnOf[a] < self.columnOf[b] end)
    self.crowd:on('complete', function(ended)
        self.jabs = self.jabs + #ended
        for _, index in ipairs(ended) do
            self.crowd:play(index, 'punch')
        end
    end, {owner = self})

    local margin = math.max(dx, dy) * 3
    self.meadow = Meadow(m.rect(-margin, -margin, self.size[1] + margin * 2, self.size[2] + margin * 2), math.max(dx, dy) * 1.6)
    self.wavePosition = nil
end

-- A wave sweeps across the crowd from the left, and every fighter it passes throws a single jab.
function Crowd:wave()
    self.wavePosition = 0
    self.waveNext = 1
end

function Crowd:resize(area)
    self:build()
end

function Crowd:update(dt)
    Crowd.super.update(self, dt)
    if not self.area then
        return
    end
    if self.wavePosition then
        self.wavePosition = self.wavePosition + self.size[1] / Crowd.waveSeconds * dt
        while self.waveNext <= #self.order and self.columnOf[self.order[self.waveNext]] <= self.wavePosition do
            self.crowd:play(self.order[self.waveNext], 'jab', true)
            self.waveNext = self.waveNext + 1
        end
        if self.waveNext > #self.order then
            self.wavePosition = nil
        end
    end
    self.crowd:update(dt)

    -- The camera centers the crowd and leaves a margin of meadow around it.
    local margin = Crowd.spacing[1] * Crowd.scale * Crowd.margin * 2
    local zoom = math.min(1.5, self.area.width / (self.size[1] + margin * 2), self.area.height / (self.size[2] + margin * 2))
    self.camera.zoom = {zoom, zoom}
    self.camera.position = {self.size[1] / 2 - self.area.width / zoom / 2, self.size[2] / 2 - self.area.height / zoom / 2}
    self:status(string.format('%s fighters in one batch, and the whole frame takes %d draw calls. Jabs that ended: %s.', Crowd.grouped(self.count), graphics2d.stats().drawCalls, Crowd.grouped(self.jabs)))
end

-- Writes a count with a comma between its thousands, the way the stepper shows the counts.
function Crowd.grouped(count)
    local text = tostring(count)
    while true do
        local grouped, changed = text:gsub('^(%d+)(%d%d%d)', '%1,%2')
        text = grouped
        if changed == 0 then
            return text
        end
    end
end

function Crowd:draw(area)
    self.meadow:draw()
    self.shadows:draw({layer = 0})
    self.crowd:draw({layer = 1})
end

return Crowd
