-- Villagers that pick what to do with a utility selector: hunger, tiredness and boredom rise over time, response curves turn them into scores, and the best option wins with a little randomness.
local ai = require('haylen.ai')
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Utility = haylen.class('Utility', AlgorithmTest)

Utility.code = [[
local function hunger(villager) return villager.hunger end
local brain = ai.newUtilitySelector({
    {name = 'eat', considerations = {{input = hunger, curve = {shape = 'logistic', slope = 10, shift = 0.6}}}},
    {name = 'work', weight = 0.7, considerations = {{input = tiredness, curve = {slope = -1, offset = 1}}}},
})
local task = brain:choose(villager, random, 0.85)  -- Picks among the options within 85 percent of the best.]]

local kSpeed = 170
-- The places stand in the corners of the village, each painted in a cell of the sheet `algorithms/places.png`, with spots around their front where villagers stand.
local kPlaces = {
    eat = {x = -480, y = -110, cell = 0, label = 'Tavern'},
    sleep = {x = 480, y = -110, cell = 1, label = 'Cottage'},
    play = {x = -480, y = 250, cell = 2, label = 'Park'},
    work = {x = 480, y = 250, cell = 3, label = 'Field'},
}
-- The spots stand in two rows, and the back row sits between the villagers of the front one, so no bars of needs cover each other.
local kSpots = {{-60, 40}, {60, 40}, {-180, 40}, {180, 40}, {0, 130}, {-120, 130}, {120, 130}, {240, 130}}
local kNeeds = {eat = 'hunger', sleep = 'tiredness', play = 'boredom'}
local kOptions = {eat = 'Eat', sleep = 'Sleep', play = 'Play', work = 'Work'}
local kNeedColors = {hunger = '#FFFFA44D', tiredness = '#FF9B7BE8', boredom = '#FF2FB3C4'}
local kTeams = {'blue', 'red', 'green', 'gold', 'violet', 'teal', 'blue', 'red'}
local kHeight = 128
-- Dirt paths join the places through the square in the middle of the village.
local kPaths = {{-480, -60, 0, 60, 480, -60}, {-480, 300, 0, 60, 480, 300}}

-- Needs score by curves: hunger grows urgent late, tiredness steadily and boredom early, and work appeals while every need is low.
local function selector()
    local function need(name, curve)
        return {name = name, input = function(villager) return villager[name] end, curve = curve}
    end
    return ai.newUtilitySelector({
        {name = 'eat', considerations = {need('hunger', {shape = 'logistic', slope = 10, shift = 0.6})}},
        {name = 'sleep', considerations = {need('tiredness', {shape = 'polynomial', exponent = 2})}},
        {name = 'play', weight = 0.8, considerations = {need('boredom', {shape = 'logistic', slope = 8, shift = 0.45})}},
        {name = 'work', weight = 0.7, considerations = {
            {name = 'fed', input = function(villager) return villager.hunger end, curve = {slope = -1, offset = 1}},
            {name = 'rested', input = function(villager) return villager.tiredness end, curve = {slope = -1, offset = 1}},
        }},
    })
end

function Utility:enter()
    self:frame{
        hint = 'Tap or click a villager to see its scores. Villagers walk to the place of their best option and choose again when the need is met. R or the X button makes everyone bored.',
        code = Utility.code,
        controls = {
            ui.button{id = 'hungry', text = 'Make everyone hungry', onClick = function() self:everyone('hunger') end},
            ui.button{id = 'bored', text = 'Make everyone bored', onClick = function() self:everyone('boredom') end},
            ui.formField{label = 'Scores of the chosen villager', ui.label{id = 'scores', text = '', font = 'monospace'}},
            ui.label{text = 'The bars over every villager show hunger in orange, tiredness in violet and boredom in cyan.', color = 'textMuted', font = 'caption'},
        },
        focus = 'hungry',
    }
    self.art = Art()
    self.places = assets.texture('algorithms/places.png', {filter = 'linear', mipmaps = true})
    self.grass = {source = self.art.sources.grass, width = 256, height = 256, pivotX = 0, pivotY = 0, layer = -10}
    self.random = m.random(103)
    self.brain = selector()
    self.villagers = {}
    for index = 1, 8 do
        self.villagers[index] = {x = self.random:range(-260, 260), y = self.random:range(-60, 160), hunger = self.random:nextFloat() * 0.6, tiredness = self.random:nextFloat() * 0.6, boredom = self.random:nextFloat() * 0.6, team = kTeams[index], facing = 1}
    end
    self.selected = self.villagers[1]
end

-- Gives a villager the first spot in front of its place that no other villager there holds, so they never stand on each other.
function Utility:freeSpot(villager)
    local used = {}
    for _, other in ipairs(self.villagers) do
        if other ~= villager and other.task == villager.task and other.spotIndex then
            used[other.spotIndex] = true
        end
    end
    for index = 1, #kSpots do
        if not used[index] then
            villager.spotIndex = index
            return kSpots[index]
        end
    end
    villager.spotIndex = 1
    return kSpots[1]
end

function Utility:everyone(need)
    for _, villager in ipairs(self.villagers) do
        villager[need] = 0.95
        villager.task = nil
    end
end

-- A villager chooses when it has nothing to do, walks to the place of its task and then works the need of the task down.
function Utility:live(villager, dt)
    villager.hunger = m.saturate(villager.hunger + dt * 0.035)
    villager.tiredness = m.saturate(villager.tiredness + dt * 0.025)
    villager.boredom = m.saturate(villager.boredom + dt * 0.045)
    if villager.task == nil then
        villager.task = self.brain:choose(villager, self.random, 0.85) or 'work'
        villager.spot = self:freeSpot(villager)
    end
    local place = kPlaces[villager.task]
    local tx, ty = place.x + villager.spot[1], place.y + villager.spot[2]
    local dx, dy = tx - villager.x, ty - villager.y
    local distance = math.sqrt(dx * dx + dy * dy)
    if distance > 4 then
        local step = math.min(distance, kSpeed * dt)
        villager.x, villager.y = villager.x + dx / distance * step, villager.y + dy / distance * step
        villager.facing = dx < 0 and -1 or 1
        villager.walking = true
        return
    end
    villager.walking = false
    local need = kNeeds[villager.task]
    if need then
        villager[need] = math.max(0, villager[need] - dt * 0.4)
        if villager[need] <= 0.05 then
            villager.task = nil
        end
    else
        villager.worked = (villager.worked or 0) + dt
        if villager.worked > 3 then
            villager.worked, villager.task = 0, nil
        end
    end
end

function Utility:update(dt)
    Utility.super.update(self, dt)
    if input.pressed('reset') then
        self:everyone('boredom')
    end
    for _, villager in ipairs(self.villagers) do
        self:live(villager, dt)
    end
    if self.pointer.pressed then
        for _, villager in ipairs(self.villagers) do
            if math.abs(villager.x - self.pointer.worldX) < 40 and self.pointer.worldY < villager.y + 10 and self.pointer.worldY > villager.y - kHeight then
                self.selected = villager
            end
        end
    end
    local villager = self.selected
    local lines = {}
    for _, option in ipairs(self.brain.options) do
        lines[#lines + 1] = string.format('%-6s %4.2f', kOptions[option], self.brain:score(option, villager))
    end
    local scores = table.concat(lines, '\n')
    if scores ~= self.scores then
        self.scores = scores
        self:set('scores', {text = scores})
    end
    self:status(string.format('The chosen villager is %.2f hungry, %.2f tired and %.2f bored, and goes to %s', villager.hunger, villager.tiredness, villager.boredom, villager.task and kOptions[villager.task]:lower() or 'choose'))
end

-- Things lower in the village draw in front, so a layer follows the line a thing stands on.
function Utility.depth(y)
    return math.floor(y + 600)
end

function Utility:draw(area)
    local bounds = graphics2d.canvasBounds()
    for y = bounds.y - (bounds.y % 256), bounds:bottom(), 256 do
        for x = bounds.x - (bounds.x % 256), bounds:right(), 256 do
            graphics2d.draw(self.art.terrain, x, y, self.grass)
        end
    end
    for _, path in ipairs(kPaths) do
        graphics2d.drawStroke(path, {width = 74, color = '#FF8A6A3E', join = 'round', cap = 'round', layer = -9})
        graphics2d.drawStroke(path, {width = 62, color = '#FFD8B378', join = 'round', cap = 'round', layer = -8})
    end
    for _, place in pairs(kPlaces) do
        Art.shadow(place.x, place.y, 260, -1)
        graphics2d.draw(self.places, place.x, place.y + 8, {source = {place.cell * 256, 0, 256, 256}, width = 260, height = 260, pivotY = 1, layer = Utility.depth(place.y)})
        self.art:label(place.label, place.x, place.y - 268, 24, '#FFFFFFFF', 2000)
    end

    -- Villagers lower on the screen stand in front, and each carries its three needs as pills over its head.
    for _, villager in ipairs(self.villagers) do
        local x, y = villager.x, villager.y
        local bob = villager.walking and math.abs(math.sin(haylen.elapsed() * 10 + x)) * 4 or 0
        local depth = Utility.depth(y)
        Art.shadow(x, y, 54, depth - 1)
        self.art:character(x, y - bob, kHeight, villager.team, depth, villager.facing < 0)
        if villager == self.selected then
            graphics2d.drawShape({x - 36, y - 11, 72, 22}, {kind = 'ellipse', color = '#00000000', borderWidth = 4, borderColor = '#FFFFC93C', layer = depth})
        end
        for index, need in ipairs({'hunger', 'tiredness', 'boredom'}) do
            local top = y - kHeight - 36 + (index - 1) * 12
            graphics2d.drawShape({x - 28, top, 56, 9}, {radius = 4.5, color = '#D0151824', layer = 2001})
            if villager[need] > 0.02 then
                graphics2d.drawShape({x - 28, top, math.max(9, 56 * villager[need]), 9}, {radius = 4.5, color = kNeedColors[need], layer = 2002})
            end
        end
    end
end

return Utility
