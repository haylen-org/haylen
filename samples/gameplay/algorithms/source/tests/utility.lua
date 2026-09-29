-- Villagers that pick what to do with a utility selector: hunger, tiredness and boredom rise over time, response curves turn them into scores, and the best option wins with a little randomness.
local haylen = require('haylen')
local ai = require('haylen.ai')
local graphics2d = require('haylen.graphics2d')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local sample = require('sample')

local Utility = haylen.class('Utility', sample.Test)

local kSpeed = 170
local kPlaces = {
    eat = {x = -520, y = -250, color = '#55FFB74D', label = 'Tavern'},
    sleep = {x = 520, y = -250, color = '#557E57C2', label = 'Houses'},
    play = {x = -520, y = 260, color = '#554DD0E1', label = 'Park'},
    work = {x = 520, y = 260, color = '#558D6E63', label = 'Field'},
}
local kNeeds = {eat = 'hunger', sleep = 'tiredness', play = 'boredom'}

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
    Utility.super.enter(self, {
        hint = 'Tap or click a villager to see its scores. Villagers walk to the place of their best option and choose again when the need is met.',
        controls = {
            ui.button{id = 'hungry', text = 'Make everyone hungry', onClick = function() self:everyone('hunger') end},
            ui.button{id = 'bored', text = 'Make everyone bored', onClick = function() self:everyone('boredom') end},
        },
        stats = true,
        focus = 'hungry',
    })
    self.random = m.random(103)
    self.brain = selector()
    self.villagers = {}
    for index = 1, 8 do
        self.villagers[index] = {x = self.random:range(-300, 300), y = self.random:range(-200, 200), hunger = self.random:float() * 0.6, tiredness = self.random:float() * 0.6, boredom = self.random:float() * 0.6, color = m.hsv(index / 8, 0.5, 0.95):toHex()}
    end
    self.selected = self.villagers[1]
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
        villager.spot = {self.random:range(-60, 60), self.random:range(-40, 40)}
    end
    local place = kPlaces[villager.task]
    local tx, ty = place.x + villager.spot[1], place.y + villager.spot[2]
    local dx, dy = tx - villager.x, ty - villager.y
    local distance = math.sqrt(dx * dx + dy * dy)
    if distance > 4 then
        local step = math.min(distance, kSpeed * dt)
        villager.x, villager.y = villager.x + dx / distance * step, villager.y + dy / distance * step
        return
    end
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

function Utility:exit()
    Utility.super.exit(self)
    self.villagers, self.brain = nil, nil
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
            if math.abs(villager.x - self.pointer.worldX) < 30 and math.abs(villager.y - self.pointer.worldY) < 30 then
                self.selected = villager
            end
        end
    end
    local villager = self.selected
    local lines = {}
    for _, option in ipairs(self.brain.options) do
        lines[#lines + 1] = string.format('%-6s %.2f', option, self.brain:score(option, villager))
    end
    self:showStats(string.format('selected villager\nhunger %.2f\ntiredness %.2f\nboredom %.2f\ntask %s\n%s', villager.hunger, villager.tiredness, villager.boredom, villager.task or '-', table.concat(lines, '\n')))
end

function Utility:render()
    self:beginWorld()
    for _, place in pairs(kPlaces) do
        graphics2d.drawRect({place.x - 110, place.y - 80, 220, 160}, place.color)
        graphics2d.drawText(nil, place.label, place.x, place.y - 110, {size = 28, anchor = {0.5, 0.5}})
    end
    for _, villager in ipairs(self.villagers) do
        local x, y = villager.x, villager.y
        graphics2d.drawCircle(x, y, 16, villager.color, {layer = 1})
        if villager == self.selected then
            graphics2d.drawRing(x, y, 24, 3, '#FFFFFFFF', {layer = 1})
        end
        for index, need in ipairs({'hunger', 'tiredness', 'boredom'}) do
            graphics2d.drawRect({x - 20, y - 34 - index * 6, 40 * villager[need], 4}, ({'#FFFFB74D', '#FF7E57C2', '#FF4DD0E1'})[index], {layer = 2})
        end
    end
end

return Utility
