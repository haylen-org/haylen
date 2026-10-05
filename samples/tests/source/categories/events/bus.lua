-- Event bus: damage events on player and enemy channels, a listener for every channel, a filter for big hits, a shield that consumes events with a high priority, and events posted for the end of the frame.
local events = require('haylen.events')
local haylen = require('haylen')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Bus = haylen.class('Bus', EventsTest)

local kCode = [[
events.on('damage', playerHud, {channel = 'player'})  events.on('damage', anyHud)
events.on('damage', bigHit, {filter = function(amount) return amount >= 20 end})
events.on('damage', function() if shield.up then return true end end, {priority = 10})  -- The value `true` consumes it.
local consumed = events.emitTo('player', 'damage', 8)  events.postTo('enemy', 'damage', 25)  -- Delivered after rendering.]]

function Bus:enter()
    local journal = Journal()
    self.journal = journal
    self.shield = false
    events.on('damage', function(amount) journal:add('Player HUD takes ' .. amount, Test.accent) end, {owner = self, channel = 'player'})
    events.on('damage', function(amount) journal:add('Enemy HUD takes ' .. amount, Test.red) end, {owner = self, channel = 'enemy'})
    events.on('damage', function(amount) journal:add('Combat log hears ' .. amount .. ' on any channel') end, {owner = self})
    events.on('damage', function(amount) journal:add('Big hit! ' .. amount .. ' passes the filter', Test.warm) end, {owner = self, filter = function(amount) return amount >= 20 end})
    events.on('damage', function(amount)
        if self.shield then
            journal:add('Shield consumes ' .. amount .. ' (priority 10)', Test.green)
            return true
        end
    end, {owner = self, priority = 10})

    self:frame({
        hint = 'Compare emit, which delivers at once, with post, which waits for the end of the frame. The frame number is on the left.',
        code = kCode,
        controls = {
            ui.button{id = 'player', text = 'Hit the player for 8', variant = 'primary', onClick = function() self:emit('player', 8) end},
            ui.button{id = 'enemy', text = 'Hit an enemy for 25', onClick = function() self:emit('enemy', 25) end},
            ui.button{id = 'none', text = 'Emit 12 on no channel', onClick = function() self:emit(nil, 12) end},
            ui.button{id = 'post', text = 'Post 30 to the player', onClick = function() self:post('player', 30) end},
            ui.toggle{id = 'shield', text = 'Shield up', onChange = function(event) self.shield = event.checked end},
        },
        focus = 'player',
    })
end

function Bus:emit(channel, amount)
    local consumed
    if channel then
        consumed = events.emitTo(channel, 'damage', amount)
    else
        consumed = events.emit('damage', amount)
    end
    self.journal:add(string.format('Emit %s on %s returned "%s"', amount, channel and 'the channel "' .. channel .. '"' or 'no channel', consumed), Test.muted)
end

function Bus:post(channel, amount)
    events.postTo(channel, 'damage', amount)
    self.journal:add('Posted ' .. amount .. ' to the channel "' .. channel .. '", nothing heard it yet', Test.muted)
end

function Bus:update(dt)
    Bus.super.update(self, dt)
    for _, topic in ipairs(events.topics()) do
        if topic.name == 'damage' then
            self:status(string.format('Event "damage": listeners %d, emissions %d, shield %s', topic.listeners, topic.emissions, self.shield and 'up' or 'down'))
        end
    end
end

function Bus:draw(area)
    self.journal:draw(24, 20, area.height - 40, 28)
end

return Bus
