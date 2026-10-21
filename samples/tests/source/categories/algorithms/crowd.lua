-- A crowd that steps hundreds of agents with ORCA, so they pass each other and the pillars without touching, plus the flocking weights of separation, alignment and cohesion.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local navigation2d = require('haylen.navigation2d')
local profiler = require('haylen.debug')
local ui = require('haylen.ui')

local AlgorithmTest = require('categories.algorithms.algorithm-test')
local Art = require('categories.algorithms.art')

local Crowd = haylen.class('Crowd', AlgorithmTest)

Crowd.code = [[
local crowd = navigation2d.newCrowd({separation = 0.2, alignment = 0, cohesion = 0})
crowd:addObstacle({{-40, -40}, {40, -40}, {40, 40}, {-40, 40}})
local id = crowd:addAgent({x = -650, y = 120, radius = 9, maxSpeed = 120})
crowd:setTarget(id, 650, 120)
crowd:step(dt)  -- Moves every agent without overlaps.]]

local kAgents = 360
local kRadius = 9
local kPillars = {{-250, -120}, {250, 120}, {0, 0}, {-250, 160}, {250, -160}}
local kArena = {-770, -410, 1540, 820}
local kWalls = {{-820, -460, 1640, 50}, {-820, 410, 1640, 50}, {-820, -460, 50, 920}, {770, -460, 50, 920}}

function Crowd:enter()
    self:frame{
        hint = 'Pick where the agents go. With the pointer mode they flock to the pointer, and the sliders weigh the flocking forces on top of the collision avoidance. R or the X button sends them again.',
        code = Crowd.code,
        controls = {
            ui.formField{label = 'Where the agents go', ui.radioGroup{id = 'mode', items = {{id = 'swap', text = 'Swap sides'}, {id = 'circle', text = 'Cross a circle'}, {id = 'pointer', text = 'Follow the pointer'}}, selected = 'swap', onChange = function(event) self:setMode(event.value) end}},
            ui.formField{label = 'Separation', ui.slider{id = 'separation', value = 0.2, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.separation = event.value end}},
            ui.formField{label = 'Alignment', ui.slider{id = 'alignment', value = 0, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.alignment = event.value end}},
            ui.formField{label = 'Cohesion', ui.slider{id = 'cohesion', value = 0, min = 0, max = 2, showValue = true, onChange = function(event) self.crowd.cohesion = event.value end}},
        },
        focus = 'mode',
    }
    self.random = m.random(71)
    self.art = Art()
    self.crowd = navigation2d.newCrowd({separation = 0.2})
    for _, pillar in ipairs(kPillars) do
        local x, y = pillar[1], pillar[2]
        self.crowd:addObstacle({{x - 40, y - 40}, {x + 40, y - 40}, {x + 40, y + 40}, {x - 40, y + 40}})
    end
    -- The walls of the arena keep the agents inside it.
    for _, wall in ipairs(kWalls) do
        local x, y, width, height = wall[1], wall[2], wall[3], wall[4]
        self.crowd:addObstacle({{x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}})
    end
    self.agents = {}
    for index = 1, kAgents do
        local left = index % 2 == 0
        local x = (left and -650 or 650) + self.random:range(-80, 80)
        local y = self.random:range(-350, 350)
        self.agents[index] = {id = self.crowd:addAgent({x = x, y = y, radius = kRadius, maxSpeed = self.random:range(90, 140)}), left = left, angle = index / kAgents * math.pi * 2}
    end
    self.disc = {kind = 'ellipse', color = nil, borderWidth = 2, borderColor = '#FF15172A', layer = 1}
    self.arrow = {{0, 0}, {0, 0}, {0, 0}}
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
            self.crowd:setPosition(agent.id, math.cos(agent.angle) * 360, math.sin(agent.angle) * 360)
            self.crowd:setTarget(agent.id, -math.cos(agent.angle) * 360, -math.sin(agent.angle) * 360)
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
    self:status(string.format('%d agents, stepped in %.3f ms', self.crowd.agentCount, self:timing('crowd')))
end

-- Each agent is a disc with a dark rim and a small arrow of its heading, in the color of the side it started on, among stone pillars on a floor with a rim.
function Crowd:draw(area)
    graphics2d.drawShape(kArena, {radius = 24, color = '#FF242C3E', borderWidth = 4, borderColor = '#FF3A4560', layer = -1})
    for _, pillar in ipairs(kPillars) do
        self.art:tile('stone', pillar[1] - 40, pillar[2] - 40, 80, 80, 0)
        graphics2d.drawShape({pillar[1] - 42, pillar[2] - 42, 84, 84}, {radius = 8, color = '#00000000', borderWidth = 4, borderColor = '#FF1E1B33', layer = 0})
    end
    local disc, arrow = self.disc, self.arrow
    for _, agent in ipairs(self.agents) do
        local position = self.crowd:position(agent.id)
        local velocity = self.crowd:velocity(agent.id)
        disc.color = agent.left and '#FF2FB3C4' or '#FFFFA44D'
        graphics2d.drawShape({position.x - kRadius, position.y - kRadius, kRadius * 2, kRadius * 2}, disc)
        local speed = math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y)
        if speed > 5 then
            local dx, dy = velocity.x / speed, velocity.y / speed
            arrow[1][1], arrow[1][2] = position.x + dx * 6, position.y + dy * 6
            arrow[2][1], arrow[2][2] = position.x - dx * 3 - dy * 4, position.y - dy * 3 + dx * 4
            arrow[3][1], arrow[3][2] = position.x - dx * 3 + dy * 4, position.y - dy * 3 - dx * 4
            graphics2d.drawPolygon(arrow, '#FF15172A', {layer = 2})
        end
    end
end

return Crowd
