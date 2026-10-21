-- Owners: guards listen to an alarm signal and an alarm event with themselves as the owner, and a banner GUI owns a listener, so removing a guard or unmounting the banner disconnects what they held.
local assets = require('haylen.assets')
local events = require('haylen.events')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local signal = require('haylen.signal')
local timer = require('haylen.timer')
local ui = require('haylen.ui')

local EventsTest = require('categories.events.events-test')
local Journal = require('harness.journal')
local Test = require('harness.test')

local Owners = haylen.class('Owners', EventsTest)

local kCode = [[
alarm:connect(function() guard.alerts = guard.alerts + 1 end, {owner = guard})
events.on('alarmRaised', function() guard.heard = guard.heard + 1 end, {owner = guard})
events.on('alarmRaised', showBanner, {owner = bannerGui})  -- Ends when the banner unmounts.
guards[#guards] = nil
collectgarbage()  -- The listeners of that guard end with it.]]

local kMostGuards = 8
local kFlash = 0.6

function Owners:enter()
    self.journal = Journal()
    self.alarm = signal.new('demo.alarm')
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.guards = {}
    self.names = 0
    self.flash = 0
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
    -- The alarm rings once by itself, so the counts of the guards and the banner show before the first press.
    timer.after(1.5, function() self:raise() end, {owner = self})
end

-- A guard is a plain table that owns its two listeners, so they end once the garbage collector frees it.
function Owners:addGuard()
    if #self.guards >= kMostGuards then
        return
    end
    self.names = self.names + 1
    local guard = {name = 'Guard ' .. self.names, alerts = 0, heard = 0}
    self.alarm:connect(function() guard.alerts = guard.alerts + 1 end, {owner = guard})
    events.on('alarmRaised', function() guard.heard = guard.heard + 1 end, {owner = guard})
    table.insert(self.guards, guard)
    self.journal:add(guard.name .. ' connects to the signal and the event.', Test.green)
    self:set('add', {enabled = #self.guards < kMostGuards})
end

function Owners:remove()
    local guard = table.remove(self.guards)
    if guard then
        self.journal:add(guard.name .. ' is dropped and collected.', Test.red)
        -- The local lets go too, so the collection frees the guard.
        guard = nil
        collectgarbage()
    end
    self:set('add', {enabled = true})
end

-- The banner is a GUI of its own in the corner of the play area, with a listener it owns.
function Owners:showBanner(visible)
    if visible and not self.banner then
        self.banner = ui.mount(ui.card{id = 'banner', anchor = 'bottomRight', gap = 8,
            ui.label{text = 'The banner listens', font = 'heading', color = 'warningText'},
            ui.label{id = 'text', text = 'It owns a listener of "alarmRaised", which ends when it unmounts.', color = 'textMuted'},
        }, {owner = self, layer = 1})
        self:keepInRegion(self.banner, 'banner', 24)
        self:placeBanner()
        local banner, heard = self.banner, 0
        events.on('alarmRaised', function()
            heard = heard + 1
            banner:set('text', {text = string.format('It heard the alarm %d %s.', heard, heard == 1 and 'time' or 'times')})
        end, {owner = banner})
        self.journal:add('The banner mounts with a listener it owns.', Test.green)
    elseif not visible and self.banner then
        self.banner:unmount()
        self.banner = nil
        self.journal:add('The banner unmounts, and its listener ends with it.', Test.red)
    end
end

function Owners:placeBanner()
    if self.banner and self.area then
        self.banner:set('banner', {width = self.area.width / 2 - 36})
    end
end

function Owners:resize(area)
    self:placeBanner()
end

function Owners:raise()
    self.alarm:emit()
    events.emit('alarmRaised')
    self.flash = kFlash
    self.journal:add(string.format('The alarm rings for %d signal listeners and %d event listeners.', self.alarm.size, self:eventListeners()), Test.warm)
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
    self.flash = math.max(0, self.flash - dt)
    local listeners = self:eventListeners()
    self:status(string.format('%d guards. The signal has %d listeners and the event "alarmRaised" has %d.', #self.guards, self.alarm.size, listeners))
end

function Owners:drawGuard(guard, x, y, size)
    local shake = self.flash > 0 and math.sin(self.flash * 60) * 4 or 0
    graphics2d.drawShape({x - size * 0.32, y - 10, size * 0.64, 20}, {kind = 'ellipse', color = '#55000000', layer = 1})
    graphics2d.draw(self.knight, x + shake, y, {width = size, height = size, pivotY = 0.97, flash = {1, 0.85, 0.3, self.flash / kFlash * 0.6}, layer = 2})
    self:label(guard.name, x, y + 26, {size = 24})
    self:pill('Signal ' .. guard.alerts, x - 4, y + 64, '#FF8FB0FF', {size = 18})
    self:pill('Event ' .. guard.heard, x - 4, y + 96, '#FF6FDCA0', {size = 18})
end

function Owners:draw(area)
    local margin = 24
    self:label('Guards that own their listeners', margin, 48, {anchor = {0, 0.5}, size = 26})
    if self.flash > 0 then
        self:icon('bell', margin + 430, 48 + math.sin(self.flash * 40) * 4, 56)
    end
    local room = area.width - margin * 2
    local spacing = room / kMostGuards
    local size = math.min(150, spacing * 0.95)
    local feet = EventsTest.counterRoom + size * 0.95
    for index, guard in ipairs(self.guards) do
        self:drawGuard(guard, area.width / 2 + spacing * (index - (#self.guards + 1) / 2), feet, size)
    end
    if #self.guards == 0 then
        graphics2d.drawText(nil, 'No guard is left. Add one to hear the alarm again.', area.width / 2, feet - size * 0.4, {size = 26, color = Test.muted, anchor = {0.5, 0.5}, layer = 2})
    end
    local top = feet + 130
    self:logPanel(self.journal, {margin, top, area.width / 2 - margin - 12, area.height - top - margin}, 'What happened')
end

return Owners
