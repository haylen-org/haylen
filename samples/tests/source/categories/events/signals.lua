-- Signals: listeners with a priority, a one-shot listener, a deferred listener, a blocked signal and a blocked connection, and a listener that disconnects another one while the signal emits. Each listener is a card that lights up when it runs, numbered in the order of the emit.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local signal = require('haylen.signal')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Signals = haylen.class('Signals', EventsTest)

local kCode = [[
local hit = signal.new('demo.hit')
hit:connect(shield, {priority = 10})
local healthBar = hit:connect(showDamage)
hit:connect(unlockAchievement, {once = true})
hit:connect(save, {deferred = true})  -- Runs at the end of the frame.
healthBar.blocked = true]]

-- The listeners in the order an emit runs them, with the icon and the rule of each.
local kListeners = {
    {id = 'shield', name = 'Shield', icon = 'shield', rule = 'Priority 10'},
    {id = 'combo', name = 'Combo breaker', icon = 'bolt', rule = 'Priority 5'},
    {id = 'healthBar', name = 'Health bar', icon = 'heart', rule = 'Connected first'},
    {id = 'achievement', name = 'Achievement', icon = 'trophy', rule = 'Once'},
    {id = 'sparkles', name = 'Sparkles', icon = 'sparkle', rule = 'Connected last'},
    {id = 'save', name = 'Save', icon = 'scroll', rule = 'Deferred'},
}
local kGlow = 1.6

function Signals:enter()
    self.journal = Journal()
    self.hit = signal.new('demo.hit')
    self.cards = {}
    for _, listener in ipairs(kListeners) do
        self.cards[listener.id] = {glow = 0, order = 0}
    end
    self.step = 0
    self:frame({
        hint = 'Emit and watch the order: priority first, then connection order, and the deferred listener at the end of the frame.',
        code = kCode,
        controls = {
            ui.button{id = 'emit', text = 'Emit a hit', variant = 'primary', onClick = function() self:emit() end},
            ui.toggle{id = 'blockSignal', text = 'Block the signal', onChange = function(event) self.hit.blocked = event.checked end},
            ui.toggle{id = 'blockBar', text = 'Block the health bar', onChange = function(event) self.connections.healthBar.blocked = event.checked end},
            ui.button{id = 'reconnect', text = 'Reconnect everything', onClick = function() self:connect() end},
        },
        focus = 'emit',
    })
    self:connect()
    -- The test emits once by itself, so the order of the listeners shows before the first press.
    timer.after(1.5, function() self:emit() end, {owner = self})
end

-- Marks the card of a listener as run, with its place in the emit.
function Signals:ran(id)
    self.step = self.step + 1
    local card = self.cards[id]
    card.glow, card.order = kGlow, self.step
end

-- Clears the signal and connects every listener again, each with the test as its owner.
function Signals:connect()
    local journal, hit = self.journal, self.hit
    hit:clear()
    local connections = {}
    connections.healthBar = hit:connect(function(damage)
        self:ran('healthBar')
        journal:add('The health bar shows ' .. damage .. ' damage.')
    end, {owner = self})
    connections.shield = hit:connect(function(damage)
        self:ran('shield')
        journal:add('The shield absorbs part of ' .. damage .. ', with priority 10.', Test.accent)
    end, {owner = self, priority = 10})
    connections.achievement = hit:connect(function()
        self:ran('achievement')
        journal:add('Achievement unlocked: the first hit. It runs only once.', Test.warm)
    end, {owner = self, once = true})
    connections.save = hit:connect(function(damage)
        self:ran('save')
        journal:add('The game saves after the frame, with ' .. damage .. ' damage.', Test.green)
    end, {owner = self, deferred = true})

    -- The combo breaker runs before the sparkles and disconnects them, so the sparkles never run in this emit.
    connections.combo = hit:connect(function()
        self:ran('combo')
        if connections.sparkles.connected then
            connections.sparkles:disconnect()
            journal:add('The combo breaker disconnects the sparkles during the emit.', Test.red)
        end
    end, {owner = self, priority = 5})
    connections.sparkles = hit:connect(function()
        self:ran('sparkles')
        journal:add('Sparkles fly.')
    end, {owner = self})
    self.connections = connections

    self:set('blockSignal', {checked = false})
    self:set('blockBar', {checked = false})
    journal:add('Connected ' .. hit.size .. ' listeners.', Test.muted)
end

function Signals:emit()
    local damage = math.random(5, 30)
    self.step = 0
    self.journal:add('Emit a hit of ' .. damage .. (self.hit.blocked and ', while the signal is blocked.' or '.'), Test.ink)
    self.hit:emit(damage)
end

function Signals:update(dt)
    Signals.super.update(self, dt)
    for _, card in pairs(self.cards) do
        card.glow = math.max(0, card.glow - dt)
    end
    local hit = self.hit
    self:status(string.format('%d listeners and %d %s. The signal is %s and the health bar is %s.', hit.size, hit.emissionCount, hit.emissionCount == 1 and 'emission' or 'emissions', hit.blocked and 'blocked' or 'open', self.connections.healthBar.blocked and 'blocked' or 'open'))
end

-- Tells what keeps a listener from running, if anything.
function Signals:holdBack(id)
    local connection = self.connections[id]
    if not connection.connected then
        return id == 'achievement' and 'Used' or 'Disconnected'
    end
    if self.hit.blocked then
        return 'Signal blocked'
    end
    if connection.blocked then
        return 'Blocked'
    end
end

function Signals:drawCard(listener, rect)
    local card = self.cards[listener.id]
    local glow = math.min(1, card.glow)
    local held = self:holdBack(listener.id)
    local x = rect[1] + rect[3] / 2
    EventsTest.panel(rect, glow)
    local lift = glow * 6
    self:icon(listener.icon, x, rect[2] + rect[4] * 0.36 - lift, math.min(rect[3] * 0.5, rect[4] * 0.42), held and '#66FFFFFF' or nil)
    self:label(listener.name, x, rect[2] + rect[4] * 0.68, {size = 26, maxWidth = rect[3] - 20, color = held and Test.muted or '#FFFFFFFF'})
    self:pill(held or listener.rule, x, rect[2] + rect[4] * 0.86, held and Test.red or (listener.id == 'save' and Test.green or '#FF8FB0FF'))
    if card.order > 0 and glow > 0 then
        local badgeX, badgeY = rect[1] + 30, rect[2] + 30
        graphics2d.drawShape({badgeX - 20, badgeY - 20, 40, 40}, {kind = 'ellipse', color = EventsTest.lit, borderWidth = 2, borderColor = '#FF15172A', layer = 5})
        graphics2d.drawText(self.font, tostring(card.order), badgeX, badgeY, {size = 24, color = '#FF15172A', anchor = {0.5, 0.5}, layer = 6})
    end
end

function Signals:draw(area)
    local margin, gap = 24, 16
    self:label('The listeners of "demo.hit", in the order an emit runs them', margin, 48, {anchor = {0, 0.5}, size = 26, maxWidth = area.width - margin * 2 - 260})
    local width = (area.width - margin * 2 - gap * (#kListeners - 1)) / #kListeners
    local height = math.min(230, math.max(170, area.height * 0.36))
    for index, listener in ipairs(kListeners) do
        self:drawCard(listener, {margin + (index - 1) * (width + gap), EventsTest.counterRoom, width, height})
    end
    local top = EventsTest.counterRoom + height + gap
    self:logPanel(self.journal, {margin, top, area.width - margin * 2, area.height - top - margin}, 'What happened')
end

return Signals
