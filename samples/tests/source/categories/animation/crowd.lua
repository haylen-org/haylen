-- Animated crowd: thousands of fighters of one animated batch punch at their own speeds, advanced in C++ by one call per frame and drawn in one draw call. A wave of single jabs runs through the crowd, and each fighter goes back to punching when its jab ends, which the batch reports in one list per update.
local animation2d = require('haylen.animation2d')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local AnimationTest = require('categories.animation.animation-test')

local Crowd = haylen.class('Crowd', AnimationTest)

Crowd.counts = {500, 2000, 8000}
Crowd.spacing = 34

Crowd.code = [[
local crowd = animation2d.newAnimatedBatch(atlas.texture, {punch = punch, jab = jab})
crowd:add({x = x, y = y, animation = 'punch', speed = 0.8})
crowd:on('complete', function(list) ... end)  -- One list per update.
crowd:update(dt)  crowd:draw()  -- One call each for the whole crowd.]]

function Crowd:enter()
    local atlas = AnimationTest.atlas()
    self.punch = atlas:animation('punch')
    self.jab = atlas:animation('punch')
    self.jab.loop = 'once'
    self.countIndex = 1
    self.jabs = 0
    self:build()
    self:frame{
        code = Crowd.code,
        hint = 'Every fighter is a sprite of one batch with its own animation, time and speed.',
        controls = {
            ui.button{id = 'count', text = 'More fighters', onClick = function()
                self.countIndex = self.countIndex % #Crowd.counts + 1
                self:build()
            end},
            ui.button{id = 'wave', text = 'Send a wave of jabs', variant = 'primary', onClick = function() self:wave() end},
        },
        focus = 'wave',
    }
end

function Crowd:build()
    local count = Crowd.counts[self.countIndex]
    self.crowd = animation2d.newAnimatedBatch(self.punch.texture, {punch = self.punch, jab = self.jab})
    self.crowd:reserve(count)
    local columns = math.ceil(math.sqrt(count * 2.5))
    for index = 0, count - 1 do
        local column, row = index % columns, index // columns
        self.crowd:add({x = (column + 0.5) * Crowd.spacing, y = (row + 0.8) * Crowd.spacing * 1.2, animation = 'punch', speed = 0.6 + (index * 37 % 9) * 0.1, scaleX = 0.5, scaleY = 0.5, flipHorizontal = row % 2 == 1})
    end
    self.columns = columns
    self.crowd:on('complete', function(ended)
        self.jabs = self.jabs + #ended
        for _, index in ipairs(ended) do
            self.crowd:play(index, 'punch')
        end
    end, {owner = self})
end

-- Every fifth fighter throws a single jab, which ends on its own frame and time.
function Crowd:wave()
    for index = 1, self.crowd:size(), 5 do
        self.crowd:play(index, 'jab', true)
    end
end

function Crowd:update(dt)
    Crowd.super.update(self, dt)
    if not self.area then
        return
    end
    self.crowd:update(dt)
    local width = self.columns * Crowd.spacing
    local height = math.ceil(self.crowd:size() / self.columns) * Crowd.spacing * 1.2
    local zoom = math.min(1.5, self.area.width / (width + Crowd.spacing), self.area.height / (height + Crowd.spacing))
    self.camera.zoom = {zoom, zoom}
    local stats = graphics2d.stats()
    self:status(string.format('Fighters %d   Draw calls %d   Jabs ended %d', self.crowd:size(), stats.drawCalls, self.jabs))
end

function Crowd:draw(area)
    self.crowd:draw({layer = 1})
end

return Crowd
