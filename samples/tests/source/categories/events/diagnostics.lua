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
for _, topic in ipairs(events.topics()) do
    print(topic.name, topic.listeners, topic.emissions, topic.stale)
end
for _, entry in ipairs(signal.list()) do
    print(entry.name, entry.listeners, entry.emissions, entry.stale)
end]]

local kRow = 36
local kFlash = 0.8

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
    self.note = 'Drop an owner to compare the counts before and after the collector frees it.'
    self.previous = {}
    self.flashes = {}
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
    self.note = string.format('The event "demoPing" had %d listeners with the owner and %d right after the collection, %d of them stale.', held, freed.listeners, freed.stale)
end

function Diagnostics:update(dt)
    Diagnostics.super.update(self, dt)
    for name, flash in pairs(self.flashes) do
        self.flashes[name] = flash > dt and flash - dt or nil
    end
    self:status(string.format('The bus knows %d event names and %d named signals.', #events.topics(), #signal.list()))
end

-- Puts the rows of the demo first and the others after them by name, and flashes the rows whose counts changed.
function Diagnostics:prepare(rows, kind)
    for _, row in ipairs(rows) do
        local key = kind .. row.name
        local counts = row.listeners .. ' ' .. row.emissions .. ' ' .. row.stale
        if self.previous[key] and self.previous[key] ~= counts then
            self.flashes[key] = kFlash
        end
        self.previous[key] = counts
        row.flash = (self.flashes[key] or 0) / kFlash
        row.demo = row.name:match('^demo') ~= nil
    end
    table.sort(rows, function(a, b)
        if a.demo ~= b.demo then
            return a.demo
        end
        return a.name < b.name
    end)
    return rows
end

-- Draws the rows of one table in a card, with the names on the left, the counts aligned on the right of their columns, and the rows that do not fit counted at the bottom.
function Diagnostics:drawTable(title, rows, rect)
    EventsTest.panel(rect)
    local x, y, width = rect[1] + 24, rect[2], rect[3] - 48
    self:label(title, x, y + 34, {anchor = {0, 0.5}, size = 26, maxWidth = width})
    local columns = {{'Listeners', width - 230}, {'Emissions', width - 100}, {'Stale', width}}
    local headerY = y + 78
    graphics2d.drawText(nil, 'Name', x, headerY, {size = 20, color = Test.muted, anchor = {0, 0.5}, layer = 2})
    for _, column in ipairs(columns) do
        graphics2d.drawText(nil, column[1], x + column[2], headerY, {size = 20, color = Test.muted, anchor = {1, 0.5}, layer = 2})
    end
    local count = math.max(0, math.floor((rect[4] - 150) / kRow))
    local shown = math.min(#rows, count)
    for index = 1, shown do
        local row = rows[index]
        local top = headerY + 20 + (index - 1) * kRow
        if index % 2 == 1 then
            graphics2d.drawShape({rect[1] + 12, top, rect[3] - 24, kRow}, {radius = 8, color = '#FF1E2330', layer = 1})
        end
        if row.flash > 0 then
            graphics2d.drawShape({rect[1] + 12, top, rect[3] - 24, kRow}, {radius = 8, color = {1, 0.79, 0.24, 0.3 * row.flash}, layer = 1})
        end
        local color = row.demo and Test.accent or Test.ink
        local middle = top + kRow / 2
        graphics2d.drawText(nil, row.name, x, middle, {size = 22, color = color, anchor = {0, 0.5}, maxWidth = width - 330, wrap = false, minScale = 0.7, layer = 2})
        graphics2d.drawText(nil, tostring(row.listeners), x + columns[1][2], middle, {size = 22, color = color, anchor = {1, 0.5}, layer = 2})
        graphics2d.drawText(nil, tostring(row.emissions), x + columns[2][2], middle, {size = 22, color = color, anchor = {1, 0.5}, layer = 2})
        graphics2d.drawText(nil, tostring(row.stale), x + columns[3][2], middle, {size = 22, color = row.stale > 0 and Test.red or color, anchor = {1, 0.5}, layer = 2})
    end
    graphics2d.drawText(nil, string.format('Showing %d of %d', shown, #rows), x, rect[2] + rect[4] - 24, {size = 20, color = Test.muted, anchor = {0, 0.5}, layer = 2})
end

function Diagnostics:draw(area)
    local margin, gap = 24, 16
    local top = EventsTest.counterRoom
    local width = (area.width - margin * 2 - gap) / 2
    local height = area.height - top - margin - 64
    self:label('Live counts of the bus and the signals', margin, 48, {anchor = {0, 0.5}, size = 26})
    self:drawTable('Topics of "events.topics()"', self:prepare(events.topics(), 'topic'), {margin, top, width, height})
    self:drawTable('Signals of "signal.list()"', self:prepare(signal.list(), 'signal'), {margin + width + gap, top, width, height})
    graphics2d.drawText(nil, self.note, margin, area.height - margin - 20, {size = 24, color = Test.warm, anchor = {0, 0.5}, maxWidth = area.width - margin * 2, layer = 2})
end

return Diagnostics
