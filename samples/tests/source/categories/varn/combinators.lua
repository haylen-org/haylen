-- The combinators of Varn's "async" module, which wait on several promises at once, with a timeline of the jobs each one waits on.
local async = require('async')
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Combinators = haylen.class('Combinators', VarnTest)

Combinators.hint = 'Each bar is a job from its start to its end, every combinator starts at the left edge, and the blue mark is where the combinator answered. Run again repeats the lesson.'
Combinators.span = 1000
Combinators.colors = {running = Test.warm, resolved = Test.green, rejected = Test.red}
Combinators.stageWidth = 580
-- The lanes grow with the stage between these heights.
Combinators.lane = {18, 40}
Combinators.laneGap = 4
Combinators.groupTitle = 26
Combinators.groupGap = 8
Combinators.margin = 24
Combinators.labels = 150

Combinators.excerpts = {
    {'Combinators', [[
local names = async.all({
    self:job('Map', 300), self:job('Sounds', 600),
    self:job('Fonts', 450),
}):await()

local first = async.race({
    self:job('Slow', 500), self:job('Fast', 200),
}):await()

local value = async.any({
    self:job('Broken', 100, 'Lost.'),
    self:job('Backup', 300),
}):await()

local outcomes = async.allSettled({
    self:job('Good', 150), self:job('Bad', 250, 'Torn.'),
}):await()

local loaded = async.mapLimit(files, 2, function(file)
    return self:job(file, 150)
end):await()]]},
    {'The job of every lane', [[
function Combinators:job(label, ms, failure)
    return async.promise(function()
        async.sleep(ms):await()
        if failure then
            error(failure, 0)
        end
        return label
    end)
end]]},
}

function Combinators:run()
    self.groups = {}
    self:all()
    self:race()
    self:any()
    self:allSettled()
    self:mapLimit()
end

function Combinators:all()
    local started = self:begin('all', 'All of them', 'async.all')
    local names = async.all({
        self:job('Map', 300), self:job('Sounds', 600),
        self:job('Fonts', 450),
    }):await()

    self.group.answered = datetime.now():millis()
    local elapsed = datetime.now():millis() - started
    local text = table.concat(names, ', ')
    self:check('all', 'All of them', text == 'Map, Sounds, Fonts' and elapsed < 900, string.format('Got "%s" in input order after %d ms, the time of the slowest and not the sum.', text, elapsed))
end

function Combinators:race()
    local started = self:begin('race', 'The first to settle', 'async.race')
    local first = async.race({
        self:job('Slow', 500), self:job('Fast', 200),
    }):await()
    self.group.answered = datetime.now():millis()
    self:check('race', 'The first to settle', first == 'Fast', string.format('The job "%s" won after %d ms, and "Slow" went on to its end.', tostring(first), datetime.now():millis() - started))
end

function Combinators:any()
    self:begin('any', 'The first to resolve', 'async.any')
    local value = async.any({
        self:job('Broken', 100, 'Lost.'),
        self:job('Backup', 300),
    }):await()
    self.group.answered = datetime.now():millis()
    self:check('any', 'The first to resolve', value == 'Backup', string.format('Passed over the rejection of "Broken" and resolved with "%s".', tostring(value)))
end

function Combinators:allSettled()
    self:begin('allSettled', 'Every outcome', 'async.allSettled')
    local outcomes = async.allSettled({
        self:job('Good', 150), self:job('Bad', 250, 'Torn.'),
    }):await()

    self.group.answered = datetime.now():millis()
    local good, bad = outcomes[1], outcomes[2]
    self:check('allSettled', 'Every outcome', good.ok and good.value == 'Good' and not bad.ok and bad.error == 'Torn.', string.format('Got {ok = %s, value = "%s"} and {ok = %s, error = "%s"} without a rejection.', tostring(good.ok), tostring(good.value), tostring(bad.ok), tostring(bad.error)))
end

function Combinators:mapLimit()
    local started = self:begin('mapLimit', 'A few at a time', 'async.mapLimit')
    local files = {'Coast', 'Cave', 'Forest', 'Port', 'Ruins', 'Peak'}
    local loaded = async.mapLimit(files, 2, function(file)
        return self:job(file, 150)
    end):await()

    self.group.answered = datetime.now():millis()
    local elapsed = datetime.now():millis() - started
    self:check('mapLimit', 'A few at a time', #loaded == #files and self.group.peak == 2, string.format('Loaded %d files, never more than %d at once, in %d ms.', #loaded, self.group.peak, elapsed))
end

-- Starts the row and the lanes of a combinator, which the timeline draws from the moment it starts.
function Combinators:begin(key, title, name)
    self.results:set(key, 'waiting', title, string.format('Waiting for "%s".', name))
    self.group = {name = title, start = datetime.now():millis(), lanes = {}, running = 0, peak = 0}
    self.groups[#self.groups + 1] = self.group
    return self.group.start
end

-- Returns a promise that settles after `ms` milliseconds, with `label` or with the error `failure`, and adds the lane that draws it.
function Combinators:job(label, ms, failure)
    local group = self.group
    local lane = {label = label, start = datetime.now():millis(), state = 'running'}
    group.lanes[#group.lanes + 1] = lane
    group.running = group.running + 1
    group.peak = math.max(group.peak, group.running)
    return async.promise(function()
        async.sleep(ms):await()
        lane.finish = datetime.now():millis()
        group.running = group.running - 1
        if failure then
            lane.state = 'rejected'
            error(failure, 0)
        end
        lane.state = 'resolved'
        return label
    end)
end

-- Draws every combinator as a group of lanes, each job a bar with round ends from its start to its end, over the lines of every 250 ms, with the moment the combinator answered marked across its lanes. The lanes share the height of the stage.
function Combinators:draw(area)
    local margin, labels = Combinators.margin, Combinators.labels
    local left, right = margin + labels, area.width - margin
    local scale = (right - left) / Combinators.span
    local now = datetime.now():millis()
    local axis = area.height - margin - 20
    for step = 0, Combinators.span, 250 do
        local x = left + step * scale
        graphics2d.drawLine(x, margin, x, axis - 8, 1, Test.line)
        graphics2d.drawText(nil, tostring(step), x, axis, {size = 18, color = Test.muted, anchor = {step == 0 and 0 or step == Combinators.span and 1 or 0.5, 0}, layer = 2})
    end
    graphics2d.drawText(nil, 'Milliseconds', left - 16, axis, {size = 18, color = Test.muted, anchor = {1, 0}, layer = 2})

    local groups, lanes = self.groups or {}, 0
    for _, group in ipairs(groups) do
        lanes = lanes + #group.lanes
    end
    local free = axis - 8 - margin - #groups * (Combinators.groupTitle + Combinators.groupGap)
    local lane = math.max(Combinators.lane[1], math.min(Combinators.lane[2], lanes > 0 and free / lanes - Combinators.laneGap or 0))
    local y = margin
    for _, group in ipairs(groups) do
        graphics2d.drawText(nil, group.name, margin, y, {size = 20, color = Test.accent, layer = 2})
        y = y + Combinators.groupTitle
        local first = y
        for _, job in ipairs(group.lanes) do
            local from = left + (job.start - group.start) * scale
            local to = left + math.min(Combinators.span, (job.finish or now) - group.start) * scale
            graphics2d.drawShape({from, y, math.max(lane, to - from), lane}, {radius = lane / 2, color = Combinators.colors[job.state], layer = 1})
            graphics2d.drawText(nil, job.label, left - 12, y + lane / 2, {size = 16, color = Test.ink, anchor = {1, 0.5}, layer = 2})
            y = y + lane + Combinators.laneGap
        end
        if group.answered then
            local x = left + math.min(Combinators.span, group.answered - group.start) * scale
            graphics2d.drawShape({x - 2, first - 4, 4, y - first + 2}, {radius = 2, color = Test.accent, layer = 3})
        end
        y = y + Combinators.groupGap
    end
end

return Combinators
