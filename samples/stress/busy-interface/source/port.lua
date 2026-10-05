-- The made-up container port the dashboard watches: live metrics, vessels, containers, berths and an event feed, all simulated in plain Lua from one random generator.
local jobs = require('haylen.jobs')
local m = require('haylen.math')

local Port = {}
Port.__index = Port

Port.syllables = {'Al', 'Bre', 'Cor', 'Dal', 'Esk', 'Fen', 'Gal', 'Hol', 'Ir', 'Jas', 'Kel', 'Lun', 'Mar', 'Nor', 'Ost', 'Pel', 'Quin', 'Ros', 'Sel', 'Tor', 'Ul', 'Ves', 'Wyn', 'Zan'}
Port.nouns = {'Tide', 'Star', 'Crest', 'Wind', 'Harbor', 'Cove', 'Spirit', 'Dawn', 'Current', 'Reef', 'Breeze', 'Horizon'}
Port.terminals = {'North', 'East', 'South', 'West', 'River', 'Basin'}
Port.stages = {
    {text = 'Anchored', tone = 'neutral'},
    {text = 'Arriving', tone = 'information'},
    {text = 'Berthed', tone = 'accent'},
    {text = 'Working', tone = 'warning'},
    {text = 'Departing', tone = 'success'},
}
Port.containerStates = {
    {text = 'Stacked', tone = 'neutral'},
    {text = 'Moving', tone = 'information'},
    {text = 'Held', tone = 'warning'},
    {text = 'Released', tone = 'success'},
    {text = 'Inspection', tone = 'danger'},
}
-- The kinds of live metric, each with its unit, its range and how far it wanders per second.
Port.metricKinds = {
    {name = 'Crane', unit = 'moves per hour', low = 0, high = 45, step = 9},
    {name = 'Gate', unit = 'trucks waiting', low = 0, high = 60, step = 14},
    {name = 'Reefer block', unit = 'kW drawn', low = 120, high = 900, step = 160},
    {name = 'Rail siding', unit = 'boxes loaded', low = 0, high = 120, step = 18},
    {name = 'Yard block', unit = 'percent full', low = 20, high = 100, step = 6},
    {name = 'Berth', unit = 'percent done', low = 0, high = 100, step = 11},
}
Port.events = {
    {text = 'Crane %d finished a lift', tone = 'success'},
    {text = 'Gate %d opened a lane', tone = 'information'},
    {text = 'Reefer %d power alarm', tone = 'danger'},
    {text = 'Truck %d left the gate', tone = 'information'},
    {text = 'Berth %d is free', tone = 'success'},
    {text = 'Yard block %d nearly full', tone = 'warning'},
}

function Port.new(seed)
    local self = setmetatable({}, Port)
    self.random = m.random(seed)
    self.eventCount = 0
    self.clock = 6 * 3600
    self.berths = {}
    for index = 1, 12 do
        self.berths[index] = {id = 'berth-' .. index, vessel = self:vesselName(), progress = self.random:range(0, 1), stage = self.random:integer(2, 5)}
    end
    return self
end

function Port:vesselName()
    local random = self.random
    return Port.syllables[random:integer(1, #Port.syllables)] .. Port.syllables[random:integer(1, #Port.syllables)]:lower() .. ' ' .. Port.nouns[random:integer(1, #Port.nouns)]
end

function Port.clockText(seconds)
    local minutes = math.floor(seconds / 60) % (24 * 60)
    return string.format('%02d:%02d', minutes // 60, minutes % 60)
end

-- The live metrics of the cards, `count` of them, cycling through the kinds.
function Port:metrics(count)
    local random = self.random
    local metrics = {}
    for index = 1, count do
        local kind = Port.metricKinds[(index - 1) % #Port.metricKinds + 1]
        metrics[index] = {id = 'metric-' .. index, title = kind.name .. ' ' .. ((index - 1) // #Port.metricKinds + 1), kind = kind, value = random:range(kind.low, kind.high)}
    end
    return metrics
end

-- Moves every metric a random step within its range, scaled to the time that passed.
function Port:stepMetrics(metrics, dt)
    local random = self.random
    for _, metric in ipairs(metrics) do
        local kind = metric.kind
        metric.value = m.clamp(metric.value + random:range(-1, 1) * kind.step * math.sqrt(dt), kind.low, kind.high)
    end
end

-- The vessels of the list, under a sticky header for every terminal. It runs as a job of "haylen.jobs", which pauses every few hundred vessels once the budget of the frame is spent.
function Port:vessels(count)
    local random = self.random
    local items = {}
    local perTerminal = math.max(1, math.ceil(count / #Port.terminals))
    for index = 1, count do
        if (index - 1) % perTerminal == 0 then
            local terminal = Port.terminals[(index - 1) // perTerminal + 1]
            items[#items + 1] = {id = 'terminal-' .. terminal, type = 'terminal', title = terminal .. ' terminal'}
        end
        local stage = random:integer(1, #Port.stages)
        items[#items + 1] = {
            id = 'vessel-' .. index,
            type = 'vessel',
            name = self:vesselName(),
            stage = stage,
            status = Port.stages[stage].text,
            tone = Port.stages[stage].tone,
            progress = random:range(0, 1),
            eta = 'Due ' .. Port.clockText(self.clock + random:range(0, 86400)),
            rate = random:range(0.02, 0.12),
        }
        if index % 256 == 0 then
            jobs.checkpoint()
        end
    end
    return items
end

-- Advances the vessels whose rows show, so their bars move and their stages change while the player watches.
function Port:stepVessels(items, first, last, dt, changed)
    for index = first, last do
        local item = items[index]
        if item and item.type == 'vessel' then
            item.progress = item.progress + item.rate * dt
            if item.progress >= 1 then
                item.progress = 0
                item.stage = item.stage % #Port.stages + 1
                item.status, item.tone = Port.stages[item.stage].text, Port.stages[item.stage].tone
            end
            changed[#changed + 1] = item.id
        end
    end
end

-- The containers of the grid. It runs as a job, like the vessels.
function Port:containers(count)
    local random = self.random
    local items = {}
    for index = 1, count do
        local state = random:integer(1, #Port.containerStates)
        items[index] = {
            id = 'box-' .. index,
            code = string.format('Box %06d', index),
            block = string.format('Block %s%d, tier %d', string.char(64 + random:integer(1, 12)), random:integer(1, 40), random:integer(1, 6)),
            status = Port.containerStates[state].text,
            tone = Port.containerStates[state].tone,
        }
        if index % 256 == 0 then
            jobs.checkpoint()
        end
    end
    return items
end

-- Changes the state of a few of the containers in view.
function Port:stepContainers(items, first, last, changes, changed)
    local random = self.random
    for _ = 1, changes do
        local item = items[random:integer(first, last)]
        if item then
            local state = Port.containerStates[random:integer(1, #Port.containerStates)]
            item.status, item.tone = state.text, state.tone
            changed[#changed + 1] = item.id
        end
    end
end

-- The next line of the event feed.
function Port:nextEvent()
    local random = self.random
    self.eventCount = self.eventCount + 1
    local event = Port.events[random:integer(1, #Port.events)]
    return {id = 'event-' .. self.eventCount, text = string.format(event.text, random:integer(1, 24)), tone = event.tone, time = Port.clockText(self.clock)}
end

-- Moves the berths along and returns the rows of the berth table.
function Port:stepBerths(dt)
    local random = self.random
    self.clock = self.clock + dt * 60
    local rows = {}
    for index, berth in ipairs(self.berths) do
        if berth.stage == 4 then
            berth.progress = berth.progress + dt * random:range(0.004, 0.02)
        end
        if berth.progress >= 1 then
            berth.progress, berth.vessel = 0, self:vesselName()
            berth.stage = 2
        elseif berth.stage ~= 4 and random:chance(dt * 0.05) then
            berth.stage = berth.stage % #Port.stages + 1
        end
        rows[index] = {id = berth.id, cells = {index, berth.vessel, string.format('%d%%', math.floor(berth.progress * 100)), Port.stages[berth.stage].text}}
    end
    return rows
end

return Port
