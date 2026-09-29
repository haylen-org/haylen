-- Signals: listeners with a priority, a one-shot listener, a deferred listener, a blocked signal and a blocked connection, and a listener that disconnects another one while the signal emits.
local haylen = require('haylen')
local signal = require('haylen.signal')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local Signals = haylen.class('Signals', sample.Test)

local kCode = [[
local hit = signal.new('demo.hit')
hit:connect(shield, {priority = 10})  hit:connect(healthBar)  hit:connect(achievement, {once = true})
hit:connect(save, {deferred = true})  -- runs at the end of the frame
connection.blocked = true  hit.blocked = true  connection:disconnect()  -- also from inside a listener]]

function Signals:enter()
    self.journal = Journal()
    self.hit = signal.new('demo.hit')
    self:frame({
        hint = 'Emit and watch the order: priority first, then connection order, and the deferred listener at the end of the frame.',
        code = kCode,
        controls = {
            ui.button{id = 'emit', text = 'Emit a hit', variant = 'primary', onClick = function() self:emit() end},
            ui.toggle{id = 'blockSignal', text = 'Block the signal', onChange = function(event) self.hit.blocked = event.checked end},
            ui.toggle{id = 'blockBar', text = 'Block the health bar', onChange = function(event) self.healthBar.blocked = event.checked end},
            ui.button{id = 'reconnect', text = 'Reconnect everything', onClick = function() self:connect() end},
        },
        focus = 'emit',
    })
    self:connect()
end

-- Clears the signal and connects every listener again, each with the test as its owner.
function Signals:connect()
    local journal, hit = self.journal, self.hit
    hit:clear()
    self.healthBar = hit:connect(function(damage) journal:add('health bar shows ' .. damage .. ' damage') end, {owner = self})
    hit:connect(function(damage) journal:add('shield absorbs part of ' .. damage .. ' (priority 10)', sample.accent) end, {owner = self, priority = 10})
    hit:connect(function() journal:add('achievement: first hit (once)', sample.warm) end, {owner = self, once = true})
    hit:connect(function(damage) journal:add('save after the frame with ' .. damage .. ' (deferred)', sample.green) end, {owner = self, deferred = true})

    -- The combo listener runs before the sparkles and disconnects them, so the sparkles never run in this emit.
    local sparkles
    hit:connect(function()
        if sparkles.connected then
            sparkles:disconnect()
            journal:add('combo breaker disconnects the sparkles during the emit', sample.red)
        end
    end, {owner = self, priority = 5})
    sparkles = hit:connect(function() journal:add('sparkles') end, {owner = self})

    self:set('blockSignal', {checked = false})
    self:set('blockBar', {checked = false})
    journal:add('connected ' .. hit.size .. ' listeners', sample.muted)
end

function Signals:emit()
    local damage = math.random(5, 30)
    self.journal:add('emit(' .. damage .. ')' .. (self.hit.blocked and ' while the signal is blocked' or ''), sample.ink)
    self.hit:emit(damage)
end

function Signals:update(dt)
    Signals.super.update(self, dt)
    self:status(string.format('listeners %d   emissions %d   signal blocked %s   health bar blocked %s', self.hit.size, self.hit.emissions, self.hit.blocked, self.healthBar.blocked))
end

function Signals:draw(area)
    self.journal:draw(24, 20, area.height - 40, 28)
end

return Signals
