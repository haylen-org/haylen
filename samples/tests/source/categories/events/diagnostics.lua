-- Diagnostics: live tables of `events.topics()` and `signal.list()` while the test adds listeners, emits, and drops the owner of two listeners to compare the counts before and after the collection.
local debugging = require('haylen.debug')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local signal = require('haylen.signal')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Test = require('harness.test')

local Diagnostics = haylen.class('Diagnostics', EventsTest)

local kCode = [[
for _, topic in ipairs(events.topics()) do print(topic.name, topic.listeners, topic.emissions, topic.stale) end
for _, entry in ipairs(signal.list()) do print(entry.name, entry.listeners, entry.emissions, entry.stale) end
debug.setStatsMode('full')  -- The overlay lists the same counts and warns about dead owners.]]

-- Returns the row named `name`, or an empty row.
local function find(rows, name)
    for _, row in ipairs(rows) do
        if row.name == name then
            return row
        end
    end
    return {listeners = 0, stale = 0}
end

function Diagnostics:enter()
    self.note = ''
    self.previousMode = debugging.statsMode()
    self.score = signal.new('demo.score')
    self.health = signal.new('demo.health')
    self.score:connect(function() end, {owner = self})
    self.health:connect(function() end, {owner = self})
    self:listen('demoPing', function() end)
    self:frame({
        hint = 'Add listeners and emit to see the counts grow, then drop an owner and compare the counts before and after the collection.',
        code = kCode,
        controls = {
            ui.button{id = 'emit', text = 'Emit everything', variant = 'primary', onClick = function() self:emit() end},
            ui.button{id = 'listen', text = 'Add listeners', onClick = function() self:addListeners() end},
            ui.button{id = 'drop', text = 'Add an owner and drop it', onClick = function() self:dropOwner() end},
            ui.toggle{id = 'overlay', text = 'Debug overlay', onChange = function(event) debugging.setStatsMode(event.checked and 'full' or 'off') end},
        },
        focus = 'emit',
    })
end

function Diagnostics:exit()
    Diagnostics.super.exit(self)
    debugging.setStatsMode(self.previousMode)
end

function Diagnostics:emit()
    self.score:emit(10)
    self.health:emit(90)
    events.emit('demoPing')
end

function Diagnostics:addListeners()
    self.score:connect(function() end, {owner = self})
    events.on('demoPing', function() end, {owner = self})
end

-- Gives two listeners to a new owner and drops it. The note keeps the counts with the owner and right after the collector freed it.
function Diagnostics:dropOwner()
    local owner = {}
    self.score:connect(function() end, {owner = owner})
    events.on('demoPing', function() end, {owner = owner})
    local held = find(events.topics(), 'demoPing').listeners
    owner = nil
    collectgarbage()
    local freed = find(events.topics(), 'demoPing')
    self.note = string.format('Event "demoPing": %d listeners with the owner, %d right after the collection, %d of them stale.', held, freed.listeners, freed.stale)
end

function Diagnostics:update(dt)
    Diagnostics.super.update(self, dt)
    self:status(string.format('Event names %d, named signals %d', #events.topics(), #signal.list()))
end

-- Draws the rows of one table in columns, with the demo rows of this test highlighted.
local function drawTable(title, rows, x, y, height)
    graphics2d.drawText(nil, title, x, y, {size = 28, color = Test.warm})
    local headers = {'Name', 'Listeners', 'Emissions', 'Stale'}
    local columns = {0, 360, 480, 600}
    for index, header in ipairs(headers) do
        graphics2d.drawText(nil, header, x + columns[index], y + 40, {size = 22, color = Test.muted})
    end
    table.sort(rows, function(a, b) return a.name < b.name end)
    local count = math.min(#rows, math.floor((height - 80) / 28))
    for index = 1, count do
        local row = rows[index]
        local color = row.name:match('^demo') and Test.accent or Test.ink
        local top = y + 44 + index * 28
        graphics2d.drawText(nil, row.name, x, top, {size = 22, color = color})
        graphics2d.drawText(nil, tostring(row.listeners), x + columns[2], top, {size = 22, color = color})
        graphics2d.drawText(nil, tostring(row.emissions), x + columns[3], top, {size = 22, color = color})
        graphics2d.drawText(nil, tostring(row.stale), x + columns[4], top, {size = 22, color = row.stale > 0 and Test.red or color})
    end
end

function Diagnostics:draw(area)
    drawTable('Topics from "events.topics()"', events.topics(), 30, 20, area.height - 80)
    drawTable('Signals from "signal.list()"', signal.list(), area.width / 2 + 30, 20, area.height - 80)
    graphics2d.drawText(nil, self.note, 30, area.height - 50, {size = 24, color = Test.red})
end

return Diagnostics
