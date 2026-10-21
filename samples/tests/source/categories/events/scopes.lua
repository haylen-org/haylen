-- Scene scopes: an arena scene connects to a signal and an event, starts a timer, a tween and a task and mounts a card, all with itself as the owner, so closing it ends every one of them without a line of cleanup. A board on the play area shows what the arena owns, lit while it runs and dimmed once it ended.
local async = require('async')
local debugging = require('haylen.debug')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local signal = require('haylen.signal')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Overlay = require('categories.events.overlay')
local Test = require('harness.test')

local Scopes = haylen.class('Scopes', EventsTest)

local kCode = [[
-- In "Arena:enter", with the arena as the owner of everything:
self:listen(params.wave, onWave)
self:listen('waveStarted', onWave)
self:spawn(function() ... end)
timer.every(0.5, tick, {owner = self})
tween.to(glow, 1, {value = 1}, {owner = self})  -- All of it ends when the arena unloads.]]

-- What the arena owns, each with its icon on the board.
local kOwned = {
    {id = 'signal', name = 'Signal connection', icon = 'bell'},
    {id = 'event', name = 'Event listener', icon = 'book'},
    {id = 'timer', name = 'Timer', icon = 'bolt'},
    {id = 'tween', name = 'Tween', icon = 'sparkle'},
    {id = 'task', name = 'Task', icon = 'scroll'},
    {id = 'card', name = 'Card of the UI', icon = 'shield'},
}

-- The arena leaves the board of the test clear, so the board shows what it owns while it runs.
local Arena = haylen.class('Arena', Overlay)
Arena.dim = false

function Arena:enter(params)
    local journal, board = params.journal, params.board
    self.journal, self.board, self.ticks, self.glow = journal, board, 0, {value = 0}
    self:card('The arena', {
        ui.label{text = 'This scene owns a signal connection, an event listener, a timer, a tween, a task and this card.', color = 'textMuted'},
        ui.progress{id = 'glow', value = 0, text = 'Tween'},
        ui.label{id = 'ticks', text = 'Timer ticks 0'},
        ui.button{id = 'wave', text = 'Start a wave', onClick = params.startWave},
    }, params.place)
    self:listen(params.wave, function(number)
        board:pulse('signal')
        journal:add('The arena hears wave ' .. number .. ' on the signal.', Test.green)
    end)
    self:listen('waveStarted', function(number)
        board:pulse('event')
        journal:add('The arena hears wave ' .. number .. ' on the bus.', Test.green)
    end)
    timer.every(0.5, function()
        self.ticks = self.ticks + 1
        board:pulse('timer')
        self.gui:set('ticks', {text = 'Timer ticks ' .. self.ticks})
    end, {owner = self})
    tween.to(self.glow, 1, {value = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, onUpdate = function()
        board.tweenValue = self.glow.value
        self.gui:set('glow', {value = self.glow.value})
    end})
    self:spawn(function()
        while true do
            async.sleep(1000):await()
            board:pulse('task')
            journal:add('The arena task wakes up.', Test.muted)
        end
    end)
    board:arenaOpened()
    journal:add('The arena enters and registers everything with itself as the owner.', Test.accent)
end

function Arena:unload()
    self.board:arenaClosed()
    self.journal:add('The arena unloads, so everything it owned ended.', Test.red)
end

function Scopes:enter()
    self.journal = Journal()
    self.wave = signal.new('demo.wave')
    self.waves = 0
    self.arenaOpen = false
    self.pulses = {}
    for _, owned in ipairs(kOwned) do
        self.pulses[owned.id] = -10
    end
    self.tweenValue = 0
    self.baseline = debugging.stats().counts
    self:listen(self.wave, function(number) self.journal:add('The test hears wave ' .. number .. ' on the signal.') end)
    self:frame({
        hint = 'Open the arena, start waves from inside it, close it and start another wave: only the test still hears it.',
        code = kCode,
        controls = {
            ui.button{id = 'arena', text = 'Open the arena', variant = 'primary', onClick = function() self:openArena() end},
            ui.button{id = 'wave', text = 'Start a wave', onClick = function() self:startWave() end},
        },
        focus = 'arena',
    })
end

-- The arena card stands in the free room at the bottom of the panel of the test, so the board and the log stay in sight.
function Scopes:openArena()
    local safe = viewport.safeRect()
    local panel = self.gui:bounds('panel')
    local place = {anchor = 'bottomRight', width = panel.width - 24, margin = {0, safe.x + safe.width - panel.x - panel.width + 12, safe.y + safe.height - panel.y - panel.height + 12, 0}}
    Arena():open({journal = self.journal, board = self, wave = self.wave, place = place, startWave = function() self:startWave() end})
end

function Scopes:arenaOpened()
    self.arenaOpen = true
    self.everOpened = true
    self:pulse('card')
end

function Scopes:arenaClosed()
    self.arenaOpen = false
end

function Scopes:pulse(id)
    self.pulses[id] = haylen.elapsed()
end

function Scopes:startWave()
    self.waves = self.waves + 1
    self.journal:add(string.format('Wave %d starts for %d signal listeners and %d bus listeners.', self.waves, self.wave.size, self:busListeners()), Test.warm)
    self.wave:emit(self.waves)
    events.emit('waveStarted', self.waves)
end

function Scopes:busListeners()
    for _, topic in ipairs(events.topics()) do
        if topic.name == 'waveStarted' then
            return topic.listeners
        end
    end
    return 0
end

-- The test does not update while the arena is on top of it, so the status line changes as it renders.
function Scopes:renderUi()
    local text
    if self.arenaOpen then
        local counts = debugging.stats().counts
        text = string.format('The arena runs %d timer and %d tween of its own, and %d signal listeners and %d bus listeners hear waves.', counts.timers - self.baseline.timers, counts.tweens - self.baseline.tweens, self.wave.size, self:busListeners())
    else
        text = string.format('The arena is closed, and the test alone hears waves: %d signal listener and %d bus listeners.', self.wave.size, self:busListeners())
    end
    if text ~= self.shown then
        self.shown = text
        Test.setNode(self, 'status', {text = text})
    end
end

function Scopes:drawOwned(owned, rect)
    local pulse = self.arenaOpen and math.max(0, 1 - (haylen.elapsed() - self.pulses[owned.id]) * 1.5) or 0
    local x = rect[1] + rect[3] / 2
    EventsTest.panel(rect, pulse)
    self:icon(owned.icon, x, rect[2] + rect[4] * 0.34 - pulse * 6, math.min(rect[3] * 0.46, rect[4] * 0.4), not self.arenaOpen and '#55FFFFFF' or nil)
    self:label(owned.name, x, rect[2] + rect[4] * 0.66, {size = 24, maxWidth = rect[3] - 20, color = self.arenaOpen and '#FFFFFFFF' or Test.muted})
    self:pill(self.arenaOpen and 'Running' or (self.everOpened and 'Ended' or 'Not yet'), x, rect[2] + rect[4] * 0.84, self.arenaOpen and Test.green or Test.muted)
    if owned.id == 'tween' and self.arenaOpen then
        local width = rect[3] - 40
        graphics2d.drawShape({rect[1] + 20, rect[2] + 12, width, 10}, {radius = 5, color = '#FF15172A', layer = 3})
        graphics2d.drawShape({rect[1] + 21, rect[2] + 13, math.max(8, (width - 2) * self.tweenValue), 8}, {radius = 4, color = EventsTest.lit, layer = 4})
    end
end

function Scopes:draw(area)
    local margin, gap = 24, 16
    self:label(self.arenaOpen and 'What the arena owns while it runs' or 'The arena is closed, so it owns nothing', margin, 48, {anchor = {0, 0.5}, size = 26})
    local width = (area.width - margin * 2 - gap * (#kOwned - 1)) / #kOwned
    local height = math.min(220, math.max(170, area.height * 0.34))
    for index, owned in ipairs(kOwned) do
        self:drawOwned(owned, {margin + (index - 1) * (width + gap), EventsTest.counterRoom, width, height})
    end
    local top = EventsTest.counterRoom + height + gap
    self:logPanel(self.journal, {margin, top, area.width - margin * 2, area.height - top - margin}, 'What happened', {'shield', 'Open the arena to give it a timer, a tween, a task, two listeners and a card.'})
end

return Scopes
