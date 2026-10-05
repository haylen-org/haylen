-- Event checklist: every event that comes from the platform, marked as it fires with its count and its last value, and the events this platform never sends with the reason, so each one can be checked live on every platform.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')
local sources = require('categories.platform.event-sources')

local EventChecklist = haylen.class('EventChecklist', Test)

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

    local rows = {}
    for _, name in ipairs(sources.events) do
        local reason = never[name]
        rows[#rows + 1] = ui.row{gap = 16, align = 'start',
            ui.label{text = name, font = 'monospace', width = 380},
            ui.label{id = name, grow = 1, text = reason and 'Never here. ' .. reason or 'Waiting.', color = reason and 'textMuted' or 'text'},
        }
        self:listen(name, function(value)
            self:record(name, value)
        end)
    end

    self:frame{
        hint = string.format('The checklist of "%s". Switch apps, resize, turn or fold the device, open the keyboard, change the network, the theme or the audio output, and connect a gamepad.', self.platform),
        content = {ui.scroll{grow = 1, ui.column{gap = 10, padding = {0, 24, 0, 0}, children = rows}}},
    }
end

function EventChecklist:record(name, value)
    self.counts[name] = (self.counts[name] or 0) + 1
    local detail = describe(value)
    self:set(name, {text = string.format('Fired %d %s%s', self.counts[name], self.counts[name] == 1 and 'time' or 'times', detail ~= '' and ', last with ' .. detail .. '.' or '.'), color = 'successText'})
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
