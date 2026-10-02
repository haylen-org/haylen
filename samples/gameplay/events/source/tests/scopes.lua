-- Scene scopes: an arena scene connects to a signal and an event, starts a timer, a tween and a task and mounts a card, all with itself as the owner, so closing it ends every one of them without a line of cleanup.
local async = require('async')
local debugging = require('haylen.debug')
local events = require('haylen.events')
local haylen = require('haylen')
local signal = require('haylen.signal')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local Scopes = haylen.class('Scopes', sample.Test)

local kCode = [[
function Arena:enter(params)
    self:listen(params.wave, onWave)  self:listen('waveStarted', onWave)  self:spawn(function() ... end)
    timer.every(0.5, tick, {owner = self})  tween.to(glow, 1, {value = 1}, {owner = self})  ui.mount(card, {owner = self})
end  -- No `exit` or `unload` needed: everything ends when the arena unloads.]]

local Arena = haylen.class('Arena', sample.Overlay)

function Arena:enter(params)
    local journal = params.journal
    self.journal, self.ticks, self.glow = journal, 0, {value = 0}
    self:card('The arena', {
        ui.label{text = 'This scene owns a signal connection, an event listener, a timer, a tween, a task and this card.', color = 'textMuted'},
        ui.progress{id = 'glow', value = 0, text = 'Tween'},
        ui.label{id = 'ticks', text = 'Timer ticks 0'},
        ui.button{id = 'wave', text = 'Start a wave', onClick = params.startWave},
    })
    self:listen(params.wave, function(number) journal:add('Arena hears wave ' .. number .. ' on the signal', sample.green) end)
    self:listen('waveStarted', function(number) journal:add('Arena hears wave ' .. number .. ' on the bus', sample.green) end)
    timer.every(0.5, function()
        self.ticks = self.ticks + 1
        self.document:set('ticks', {text = 'Timer ticks ' .. self.ticks})
    end, {owner = self})
    tween.to(self.glow, 1, {value = 1}, {owner = self, loopMode = 'yoyo', repeatCount = -1, onUpdate = function() self.document:set('glow', {value = self.glow.value}) end})
    self:spawn(function()
        while true do
            async.sleep(1000):await()
            journal:add('The arena task wakes up', sample.muted)
        end
    end)
    journal:add('Arena entered and registered everything', sample.accent)
end

function Arena:unload()
    self.journal:add('Arena unloaded, so everything it owned ended', sample.red)
end

function Scopes:enter()
    self.journal = Journal()
    self.wave = signal.new('demo.wave')
    self.waves = 0
    self:listen(self.wave, function(number) self.journal:add('The test hears wave ' .. number .. ' on the signal') end)
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

function Scopes:openArena()
    sample.overlay(Arena(), {journal = self.journal, wave = self.wave, startWave = function() self:startWave() end})
end

function Scopes:startWave()
    self.waves = self.waves + 1
    self.journal:add(string.format('Wave %d starts: %d signal listeners, %d bus listeners', self.waves, self.wave.size, self:busListeners()), sample.warm)
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

function Scopes:update(dt)
    Scopes.super.update(self, dt)
    local counts = debugging.stats().counts
    self:status(string.format('Signal listeners %d   bus listeners %d   timers %d   tweens %d', self.wave.size, self:busListeners(), counts.timers, counts.tweens))
end

function Scopes:draw(area)
    self.journal:draw(24, 20, area.height - 40, 28)
end

return Scopes
