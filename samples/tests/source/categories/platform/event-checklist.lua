-- Event checklist: every event that comes from the platform, marked as it fires with its count and its last value, the state the app holds already, and the events this platform never sends with the reason, so each one can be checked live on every platform.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')
local sources = require('categories.platform.event-sources')

local EventChecklist = haylen.class('EventChecklist', Test)

EventChecklist.code = [[
events.on('appBackground', function() preferences.save() end, {owner = self})
events.on('appLowMemory', function() assets.unloadGroup('level') end, {owner = self})
events.on('systemThemeChanged', function(change) ui.setTheme(change.theme) end, {owner = self})
events.on('gamepadConnected', function(info) print(info.gamepad, info.name) end, {owner = self})
events.on('batteryChanged', function(battery) print(battery.level, battery.charging) end, {owner = self})]]

-- The events that name the state of the app, which already holds when the test opens.
EventChecklist.states = {active = 'appActive', inactive = 'appInactive', background = 'appBackground'}

-- Mac Catalyst apps run the iOS platform on a Mac.
local function platformName()
    if haylen.platform == 'ios' and system.info().os == 'macOs' then
        return 'catalyst'
    end
    return haylen.platform
end

-- Writes the value of an event in a few words.
local function describe(value)
    if type(value) ~= 'table' then
        return ''
    end
    local fields = {}
    for key, field in pairs(value) do
        if type(field) ~= 'table' then
            fields[#fields + 1] = key .. ' ' .. (type(field) == 'number' and string.format('%g', field) or tostring(field))
        end
    end
    table.sort(fields)
    return table.concat(fields, ', ')
end

function EventChecklist:enter()
    self.platform = platformName()
    self.counts = {}
    local never = sources.never[self.platform] or {}
    local holding = EventChecklist.states[haylen.appState()]

    local cards = {}
    for _, name in ipairs(sources.events) do
        local reason = never[name]
        local text, tone, detail = 'Waiting', 'warning', 'It has not fired since the test opened.'
        if reason then
            text, tone, detail = 'Never here', 'neutral', reason
        elseif name == holding then
            text, tone, detail = 'Holds now', 'information', 'The app is in this state already, so the event fires when the app comes back to it.'
        end
        cards[#cards + 1] = ui.card{align = 'stretch', gap = 4, padding = {12, 18},
            ui.row{gap = 12, ui.label{text = name, font = 'monospace', grow = 1}, ui.badge{id = name .. '-state', text = text, tone = tone}},
            ui.label{id = name, text = detail, font = 'caption', color = 'textMuted'},
        }
        self:listen(name, function(value)
            self:record(name, value)
        end)
    end

    self:frame{
        hint = string.format('The checklist of "%s". Switch apps, resize, turn or fold the device, open the keyboard, change the network, the theme or the audio output, and connect a gamepad.', self.platform),
        code = EventChecklist.code,
        content = {ui.panel{grow = 1, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.grid{columns = 2, gap = 12, children = cards}}}},
    }
end

function EventChecklist:record(name, value)
    self.counts[name] = (self.counts[name] or 0) + 1
    local detail = describe(value)
    self:set(name .. '-state', {text = string.format('Fired %d %s', self.counts[name], self.counts[name] == 1 and 'time' or 'times'), tone = 'success'})
    self:set(name, {text = detail ~= '' and 'The last one came with ' .. detail .. '.' or 'The last one came without a value.'})
    self:log('The event "%s" fired%s.', name, detail ~= '' and ' with ' .. detail or '')
end

function EventChecklist:update(dt)
    EventChecklist.super.update(self, dt)
    local fired = 0
    for _ in pairs(self.counts) do
        fired = fired + 1
    end
    self:status(string.format('The app is "%s". %d of %d events fired since the test opened.', haylen.appState(), fired, #sources.events))
end

return EventChecklist
