-- Dates and times with Varn's "datetime" module: the current instant, ISO-8601 text with offsets, calendar arithmetic, differences, boundaries, fixed offsets and Unix time.
local datetime = require('datetime')
local haylen = require('haylen')
local ui = require('haylen.ui')

local VarnTest = require('categories.varn.varn-test')

local Dates = haylen.class('Dates', VarnTest)

Dates.hint = 'The clock reads "datetime.now()" every frame, in UTC, since the module knows fixed offsets and no time zones. Run again repeats the lesson.'
Dates.clockSize = 76

Dates.excerpts = {
    {'Now', [[
local now = datetime.now()
print(now:iso(), now:format('%A %d %B %Y'))]]},
    {'Parse and compute', [[
local launch = datetime.parse('2026-06-21T09:30:00-03:00')
print(launch:iso(), launch:weekdayName(), launch:yearday())
local due = datetime.parse('2026-01-31'):add({months = 1})
local from = datetime.parse('2026-01-10')
local to = datetime.parse('2026-03-15')
print(to:diffIn(from, 'days'), to:diffIn(from, 'months'))
print(launch:startOf('week'):iso(), launch:endOf('week'):iso())
print(launch:iso(330), launch:format('%H:%M', -180))]]},
}

-- A clock in the monospace font, whose digits keep their width as they change, over the day and the ISO text of the instant.
function Dates:showcase()
    return {ui.card{align = 'stretch', gap = 6, padding = {20, 28},
        ui.label{id = 'clock', text = '', font = 'monospace', style = {fonts = {monospace = {size = Dates.clockSize}}}, textAlign = 'center', align = 'stretch'},
        ui.label{id = 'day', text = '', font = 'heading', textAlign = 'center', align = 'stretch'},
        ui.label{id = 'iso', text = '', font = 'monospace', color = 'textMuted', textAlign = 'center', align = 'stretch'},
    }}
end

function Dates:run()

    local launch = datetime.parse('2026-06-21T09:30:00-03:00')
    self:check('parse', 'Parse with an offset', launch:iso() == '2026-06-21T12:30:00Z' and launch:weekdayName() == 'Sunday' and launch:yearday() == 172, string.format('The text "2026-06-21T09:30:00-03:00" is %s in UTC, a %s, day %d of the year.', launch:iso(), launch:weekdayName(), launch:yearday()))

    local due = datetime.parse('2026-01-31'):add({months = 1})
    self:check('months', 'Calendar months', due:iso() == '2026-02-28T00:00:00Z', string.format('January 31 plus one month is %s, the last day of February, and not a day that does not exist.', due:iso()))

    local from, to = datetime.parse('2026-01-10'), datetime.parse('2026-03-15')
    local days, months = to:diffIn(from, 'days'), to:diffIn(from, 'months')
    self:check('diff', 'Differences', days == 64 and months == 2, string.format('From 2026-01-10 to 2026-03-15 there are %d days, %d whole months and %d seconds.', days, months, to:diff(from)))

    local first, last = launch:startOf('week'), launch:endOf('week')
    self:check('bounds', 'Boundaries', first:iso() == '2026-06-15T00:00:00Z' and last:iso() == '2026-06-21T23:59:59.999Z', string.format('The week of the launch starts on Monday %s and ends at %s.', first:iso(), last:iso()))

    local ahead, home = launch:iso(330), launch:format('%H:%M', -180)
    self:check('offsets', 'Fixed offsets', ahead == '2026-06-21T18:00:00+05:30' and home == '09:30', string.format('At an offset of +05:30 the launch reads %s, and at -03:00 the clock showed %s.', ahead, home))

    local epoch = datetime.fromUnix(0)
    local now = datetime.now()
    self:check('unix', 'Unix time and order', epoch:iso() == '1970-01-01T00:00:00Z' and epoch < now and datetime.min(now, epoch) == epoch, string.format('The Unix epoch is %s, now is %d seconds later, and instants compare with "<".', epoch:iso(), now:unix()))

    local ok, failure = pcall(datetime.parse, 'next Tuesday')
    self:check('invalid', 'Invalid text', not ok and failure ~= nil, 'The call raised, and "pcall" caught it: ' .. tostring(failure))
end

function Dates:update(dt)
    Dates.super.update(self, dt)
    local now = datetime.now()
    self:set('clock', {text = string.format('%s.%03d UTC', now:format('%H:%M:%S'), now:millis() % 1000)})
    local day = now:format('%A %d %B %Y')
    if day ~= self.day then
        self.day = day
        self:set('day', {text = day})
    end
    self:set('iso', {text = now:iso()})
end

return Dates
