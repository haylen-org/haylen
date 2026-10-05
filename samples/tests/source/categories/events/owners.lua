-- Owners: guards listen to an alarm signal and an alarm event with themselves as the owner, and a banner GUI owns a listener, so removing a guard or unmounting the banner disconnects what they held.
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local signal = require('haylen.signal')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Owners = haylen.class('Owners', EventsTest)

local kCode = [[
alarm:connect(function() guard.alert = guard.alert + 1 end, {owner = guard})
events.on('alarmRaised', function() guard.heard = guard.heard + 1 end, {owner = guard})
events.on('alarmRaised', showBanner, {owner = bannerGui})  -- Ends when the banner is unmounted.
guards[#guards] = nil  collectgarbage()  -- The listeners of that guard end at the end of the frame.]]

function Owners:enter()
    self.journal = Journal()
    self.alarm = signal.new('demo.alarm')
    self.guards = {}
    self.names = 0
    self:frame({
        hint = 'Raise the alarm, remove guards and unmount the banner, then raise it again. The counts drop without any disconnect call.',
        code = kCode,
        controls = {
            ui.button{id = 'alarm', text = 'Raise the alarm', variant = 'primary', onClick = function() self:raise() end},
            ui.button{id = 'add', text = 'Add a guard', onClick = function() self:addGuard() end},
            ui.button{id = 'remove', text = 'Remove a guard', variant = 'destructive', onClick = function() self:remove() end},
            ui.toggle{id = 'banner', text = 'Banner mounted', checked = true, onChange = function(event) self:showBanner(event.checked) end},
        },
        focus = 'alarm',
    })
    for _ = 1, 4 do
        self:addGuard()
    end
    self:showBanner(true)
end

-- A guard is a plain table that owns its two listeners, so they end once the garbage collector frees it.
function Owners:addGuard()
    self.names = self.names + 1
    local guard = {name = 'Guard ' .. self.names, alert = 0, heard = 0}
    self.alarm:connect(function() guard.alert = guard.alert + 1 end, {owner = guard})
    events.on('alarmRaised', function() guard.heard = guard.heard + 1 end, {owner = guard})
    table.insert(self.guards, guard)
    self.journal:add(guard.name .. ' connects to the signal and the event', Test.green)
end

function Owners:remove()
    local guard = table.remove(self.guards)
    if guard then
        self.journal:add(guard.name .. ' is dropped and collected', Test.red)
        -- The local lets go too, so the collection frees the guard.
        guard = nil
        collectgarbage()
    end
end

function Owners:showBanner(visible)
    if visible and not self.banner then
        self.banner = ui.mount(ui.label{id = 'text', text = 'The banner listens', font = 'heading', color = 'warningText', anchor = 'top', margin = {140, 0}}, {owner = self, layer = 1})
        local banner = self.banner
        events.on('alarmRaised', function() banner:set('text', {text = 'The banner heard the alarm at frame ' .. haylen.frameIndex()}) end, {owner = banner})
        self.journal:add('Banner mounted with a listener it owns', Test.green)
    elseif not visible and self.banner then
        self.banner:unmount()
        self.banner = nil
        self.journal:add('Banner unmounted', Test.red)
    end
end

function Owners:raise()
    self.alarm:emit()
    events.emit('alarmRaised')
    self.journal:add(string.format('Alarm: %d signal listeners, %d event listeners', self.alarm.size, self:eventListeners()), Test.warm)
end

function Owners:eventListeners()
    for _, topic in ipairs(events.topics()) do
        if topic.name == 'alarmRaised' then
            return topic.listeners, topic.stale
        end
    end
    return 0, 0
end

function Owners:update(dt)
    Owners.super.update(self, dt)
    local listeners, stale = self:eventListeners()
    self:status(string.format('Guards %d, signal listeners %d, event listeners %d, stale %d', #self.guards, self.alarm.size, listeners, stale))
end

function Owners:draw(area)
    for index, guard in ipairs(self.guards) do
        local x = 90 + (index - 1) * 150
        graphics2d.drawCircle(x, 90, 44, guard.alert > 0 and Test.warm or Test.accent)
        graphics2d.drawText(nil, guard.name, x, 160, {size = 22, color = Test.ink, anchor = {0.5, 0.5}})
        graphics2d.drawText(nil, guard.alert .. ' / ' .. guard.heard, x, 90, {size = 24, color = '#FF1B1E2B', anchor = {0.5, 0.5}, layer = 1})
    end
    self.journal:draw(24, 200, area.height - 220, 26)
end

return Owners
