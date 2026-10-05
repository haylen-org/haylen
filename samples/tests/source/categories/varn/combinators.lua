-- The combinators of Varn's "async" module, which wait on several promises at once, with a timeline of the jobs each one waits on.
local async = require('async')
local datetime = require('datetime')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local Test = require('harness.test')
local VarnTest = require('categories.varn.varn-test')

local Combinators = haylen.class('Combinators', VarnTest)

Combinators.hint = 'Each bar is a job from its start to its end, every combinator starts at the left edge, and the faint lines are 100 ms apart. Run again repeats the lesson.'
Combinators.span = 1000
Combinators.colors = {running = Test.warm, resolved = Test.green, rejected = Test.red}

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
    {'A job, without its timeline', [[
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

    local elapsed = datetime.now():millis() - started
    local text = table.concat(names, ', ')
    self:check('all', 'All of them', text == 'Map, Sounds, Fonts' and elapsed < 900, string.format('Got "%s" in input order after %d ms, the time of the slowest and not the sum.', text, elapsed))
end

function Combinators:race()
    local started = self:begin('race', 'The first to settle', 'async.race')
    local first = async.race({
        self:job('Slow', 500), self:job('Fast', 200),
    }):await()
    self:check('race', 'The first to settle', first == 'Fast', string.format('The job "%s" won after %d ms, and "Slow" went on to its end.', tostring(first), datetime.now():millis() - started))
end

function Combinators:any()
    self:begin('any', 'The first to resolve', 'async.any')
    local value = async.any({
        self:job('Broken', 100, 'Lost.'),
        self:job('Backup', 300),
    }):await()
    self:check('any', 'The first to resolve', value == 'Backup', string.format('Passed over the rejection of "Broken" and resolved with "%s".', tostring(value)))
end

function Combinators:allSettled()
    self:begin('allSettled', 'Every outcome', 'async.allSettled')
    local outcomes = async.allSettled({
        self:job('Good', 150), self:job('Bad', 250, 'Torn.'),
    }):await()

    local good, bad = outcomes[1], outcomes[2]
    self:check('allSettled', 'Every outcome', good.ok and good.value == 'Good' and not bad.ok and bad.error == 'Torn.', string.format('Got {ok = %s, value = "%s"} and {ok = %s, error = "%s"} without a rejection.', tostring(good.ok), tostring(good.value), tostring(bad.ok), tostring(bad.error)))
end

function Combinators:mapLimit()
    local started = self:begin('mapLimit', 'A few at a time', 'async.mapLimit')
    local files = {'Coast', 'Cave', 'Forest', 'Port', 'Ruins', 'Peak'}
    local loaded = async.mapLimit(files, 2, function(file)
        return self:job(file, 150)
    end):await()

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

function Combinators:draw(area)
    local top = self.results:draw(area)
    self:drawTimeline(area, top)
end

function Combinators:drawTimeline(area, top)
    local left, right = 260, area.width - 24
    local scale = (right - left) / Combinators.span
    local now = datetime.now():millis()
    local y = top

    for _, group in ipairs(self.groups or {}) do
        local height = #group.lanes * 22
        Test.caption(group.name, 24, y - 2, {color = Test.accent})
        for step = 0, Combinators.span, 100 do
            graphics2d.drawLine(left + step * scale, y, left + step * scale, y + height - 4, 1, Test.line, {layer = 0})
        end
        for _, lane in ipairs(group.lanes) do
            local from = left + (lane.start - group.start) * scale
            local to = left + math.min(Combinators.span, (lane.finish or now) - group.start) * scale
            graphics2d.drawRect({from, y, math.max(2, to - from), 18}, Combinators.colors[lane.state], {layer = 1})
            Test.caption(lane.label, from + 6, y, {size = 15, color = '#FF101418'})
            y = y + 22
        end
        y = y + 10
    end
end

return Combinators
