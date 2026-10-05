-- A crowd that steps hundreds of agents with ORCA, so they pass each other and the pillars without touching, plus the flocking weights of separation, alignment and cohesion.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')

local Crowd = haylen.class('Crowd', AlgorithmTest)

local kAgents = 360
local kRadius = 9
local kPillars = {{-250, -120}, {250, 120}, {0, 0}, {-250, 160}, {250, -160}}

function Crowd:enter()
    self:frame{
        hint = 'Pick where the agents go. With the pointer mode they flock to the pointer, and the sliders weigh the flocking forces on top of the collision avoidance. R or the X button sends them again.',
        controls = {
            ui.radioGroup{id = 'mode', items = {{id = 'swap', text = 'Swap sides'}, {id = 'circle', text = 'Cross a circle'}, {id = 'pointer', text = 'Follow the pointer'}}, selected = 'swap', onChange = function(event) self:setMode(event.value) end},
            ui.label{text = 'Separation', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'separation', value = 0.2, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.separation = event.value end},
            ui.label{text = 'Alignment', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'alignment', value = 0, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.alignment = event.value end},
            ui.label{text = 'Cohesion', font = 'caption', color = 'textMuted'},
            ui.slider{id = 'cohesion', value = 0, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.cohesion = event.value end},
        },
        focus = 'mode',
    }
    self.random = m.random(71)
    self.crowd = navigation2d.newCrowd({separation = 0.2})
    for _, pillar in ipairs(kPillars) do
        local x, y = pillar[1], pillar[2]
        self.crowd:addObstacle({{x - 40, y - 40}, {x + 40, y - 40}, {x + 40, y + 40}, {x - 40, y + 40}})
    end
    self.agents = {}
    for index = 1, kAgents do
        local left = index % 2 == 0
        local x = (left and -650 or 650) + self.random:range(-80, 80)
        local y = self.random:range(-380, 380)
        self.agents[index] = {id = self.crowd:addAgent({x = x, y = y, radius = kRadius, maxSpeed = self.random:range(90, 140)}), left = left, angle = index / kAgents * math.pi * 2}
    end
    self:setMode('swap')
end

-- Every agent gets a target by the mode, with a small spread so agents that want the same spot do not stand off.
function Crowd:setMode(mode)
    self.mode = mode
    for _, agent in ipairs(self.agents) do
        local position = self.crowd:position(agent.id)
        if mode == 'swap' then
            agent.left = not agent.left
            self.crowd:setTarget(agent.id, (agent.left and -650 or 650) + self.random:range(-80, 80), position.y)
        elseif mode == 'circle' then
            self.crowd:setPosition(agent.id, math.cos(agent.angle) * 390, math.sin(agent.angle) * 390)
            self.crowd:setTarget(agent.id, -math.cos(agent.angle) * 390, -math.sin(agent.angle) * 390)
        end
    end
end

function Crowd:update(dt)
    Crowd.super.update(self, dt)
    if input.pressed('reset') then
        self:setMode(self.mode)
    end
    if self.mode == 'pointer' then
        for index, agent in ipairs(self.agents) do
            local spread = (index % 12) * 6
            self.crowd:setTarget(agent.id, self.pointer.worldX + math.cos(index) * spread, self.pointer.worldY + math.sin(index) * spread)
        end
    end
    profiler.beginScope('crowd')
    self.crowd:step(dt)
    profiler.endScope()
    self:status(string.format('Agents %d, step %.3f ms', self.crowd.agentCount, self:timing('crowd')))
end

function Crowd:draw(area)
    for _, pillar in ipairs(kPillars) do
        graphics2d.drawRect({pillar[1] - 40, pillar[2] - 40, 80, 80}, '#FF607D8B')
    end
    for _, agent in ipairs(self.agents) do
        local position = self.crowd:position(agent.id)
        local velocity = self.crowd:velocity(agent.id)
        graphics2d.drawCircle(position.x, position.y, kRadius, agent.left and '#FF4DD0E1' or '#FFFFB74D', {layer = 1}, 12)
        graphics2d.drawLine(position.x, position.y, position.x + velocity.x * 0.1, position.y + velocity.y * 0.1, 2, '#AAFFFFFF', {layer = 2})
    end
end

return Crowd
