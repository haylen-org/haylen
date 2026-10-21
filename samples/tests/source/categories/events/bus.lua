-- Event bus: damage events on player and enemy channels, a listener for every channel, a filter for big hits, a shield that consumes events with a high priority, and events posted for the end of the frame. Each listener is a card that lights up and counts what it heard, and the two health bars take the damage of their channel.
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Bus = haylen.class('Bus', EventsTest)

local kCode = [[
events.on('damage', hurtPlayer, {channel = 'player'})
events.on('damage', writeCombatLog)  -- Hears every channel.
events.on('damage', bigHit, {filter = function(amount) return amount >= 20 end})
events.on('damage', function() return shield.up end, {priority = 10})  -- The value true consumes it.
local consumed = events.emitTo('player', 'damage', 8)
events.postTo('enemy', 'damage', 25)  -- Delivered at the end of the frame.]]

local kListeners = {
    {id = 'shield', name = 'Shield', icon = 'shield', rule = 'Priority 10'},
    {id = 'player', name = 'Player', icon = 'heart', rule = 'Channel "player"'},
    {id = 'enemy', name = 'Enemy', icon = 'skull', rule = 'Channel "enemy"'},
    {id = 'log', name = 'Combat log', icon = 'book', rule = 'Every channel'},
    {id = 'big', name = 'Big hit', icon = 'bolt', rule = 'Filter 20 or more'},
}
local kGlow = 1.6
local kHealth = 100

function Bus:enter()
    local journal = Journal()
    self.journal = journal
    self.shield = false
    self.cards = {}
    for _, listener in ipairs(kListeners) do
        self.cards[listener.id] = {glow = 0, heard = 0}
    end
    self.health = {player = kHealth, enemy = kHealth}
    events.on('damage', function(amount)
        self:heard('player')
        self.health.player = math.max(0, self.health.player - amount)
        journal:add('The player loses ' .. amount .. ' health.', Test.accent)
    end, {owner = self, channel = 'player'})
    events.on('damage', function(amount)
        self:heard('enemy')
        self.health.enemy = math.max(0, self.health.enemy - amount)
        journal:add('The enemy loses ' .. amount .. ' health.', Test.red)
    end, {owner = self, channel = 'enemy'})
    events.on('damage', function(amount)
        self:heard('log')
        journal:add('The combat log hears ' .. amount .. ' on any channel.')
    end, {owner = self})
    events.on('damage', function(amount)
        self:heard('big')
        journal:add('Big hit! ' .. amount .. ' passes the filter.', Test.warm)
    end, {owner = self, filter = function(amount) return amount >= 20 end})
    events.on('damage', function(amount)
        if self.shield then
            self:heard('shield')
            journal:add('The shield consumes ' .. amount .. ' before anyone else hears it.', Test.green)
            return true
        end
    end, {owner = self, priority = 10})

    self:frame({
        hint = 'Compare emit, which delivers at once, with post, which waits for the end of the frame of the same number.',
        code = kCode,
        controls = {
            ui.button{id = 'player', text = 'Hit the player for 8', variant = 'primary', onClick = function() self:emit('player', 8) end},
            ui.button{id = 'enemy', text = 'Hit the enemy for 25', onClick = function() self:emit('enemy', 25) end},
            ui.button{id = 'none', text = 'Emit 12 on no channel', onClick = function() self:emit(nil, 12) end},
            ui.button{id = 'post', text = 'Post 30 to the player', onClick = function() self:post('player', 30) end},
            ui.toggle{id = 'shield', text = 'Shield up', onChange = function(event) self.shield = event.checked end},
            ui.button{id = 'heal', text = 'Heal both', onClick = function() self.health = {player = kHealth, enemy = kHealth} end},
        },
        focus = 'player',
    })
    -- The enemy takes a hit by itself, so the listeners that hear it light up before the first press.
    timer.after(1.5, function() self:emit('enemy', 25) end, {owner = self})
end

function Bus:heard(id)
    local card = self.cards[id]
    card.glow, card.heard = kGlow, card.heard + 1
end

function Bus:emit(channel, amount)
    local consumed
    if channel then
        consumed = events.emitTo(channel, 'damage', amount)
    else
        consumed = events.emit('damage', amount)
    end
    self.journal:add(string.format('Emit %d %s: %s.', amount, channel and 'on the channel "' .. channel .. '"' or 'on no channel', consumed and 'a listener consumed it' or 'nobody consumed it'), Test.muted)
end

function Bus:post(channel, amount)
    events.postTo(channel, 'damage', amount)
    self.journal:add('Post ' .. amount .. ' to the channel "' .. channel .. '": nobody heard it yet.', Test.muted)
end

function Bus:update(dt)
    Bus.super.update(self, dt)
    for _, card in pairs(self.cards) do
        card.glow = math.max(0, card.glow - dt)
    end
    for _, topic in ipairs(events.topics()) do
        if topic.name == 'damage' then
            self:status(string.format('The event "damage" has %d listeners and %d emissions, and the shield is %s.', topic.listeners, topic.emissions, self.shield and 'up' or 'down'))
        end
    end
end

-- Draws a health bar with round ends in its groove, under the card of a channel.
function Bus.drawHealth(value, x, y, width, color)
    local height = 16
    graphics2d.drawShape({x, y, width, height}, {radius = height / 2, color = '#FF15172A', layer = 3})
    local fill = (width - 4) * value / kHealth
    if fill > 0 then
        graphics2d.drawShape({x + 2, y + 2, math.max(fill, height - 4), height - 4}, {radius = (height - 4) / 2, color = color, layer = 4})
    end
end

function Bus:drawCard(listener, rect)
    local card = self.cards[listener.id]
    local glow = math.min(1, card.glow)
    local x = rect[1] + rect[3] / 2
    local off = listener.id == 'shield' and not self.shield
    EventsTest.panel(rect, glow)
    self:icon(listener.icon, x, rect[2] + rect[4] * 0.3 - glow * 6, math.min(rect[3] * 0.46, rect[4] * 0.36), off and '#66FFFFFF' or nil)
    self:label(listener.name, x, rect[2] + rect[4] * 0.58, {size = 26, maxWidth = rect[3] - 20, color = off and Test.muted or '#FFFFFFFF'})
    self:pill(off and 'Shield down' or listener.rule, x, rect[2] + rect[4] * 0.74, off and Test.muted or '#FF8FB0FF')
    local health = self.health[listener.id]
    if health then
        Bus.drawHealth(health, rect[1] + 24, rect[2] + rect[4] - 34, rect[3] - 48, listener.id == 'player' and Test.green or Test.red)
    else
        graphics2d.drawText(nil, string.format('Heard %d', card.heard), x, rect[2] + rect[4] - 26, {size = 20, color = Test.muted, anchor = {0.5, 0.5}, layer = 3})
    end
end

function Bus:draw(area)
    local margin, gap = 24, 16
    self:label('The listeners of the event "damage"', margin, 48, {anchor = {0, 0.5}, size = 26})
    local width = (area.width - margin * 2 - gap * (#kListeners - 1)) / #kListeners
    local height = math.min(240, math.max(180, area.height * 0.38))
    for index, listener in ipairs(kListeners) do
        self:drawCard(listener, {margin + (index - 1) * (width + gap), EventsTest.counterRoom, width, height})
    end
    local top = EventsTest.counterRoom + height + gap
    self:logPanel(self.journal, {margin, top, area.width - margin * 2, area.height - top - margin}, 'What happened', {'book', 'Emit or post a hit to see who receives it.'})
end

return Bus
