-- What `haylen.system` tells about the device, its theme, its battery, its power saving mode, its time zone and its languages with their changes as they happen, and the clipboard of the system, with the answer of each copy and read and the frames it took.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')

local System = haylen.class('System', Test)

System.events = {'systemThemeChanged', 'batteryChanged', 'powerSavingChanged', 'timeZoneChanged', 'systemLanguagesChanged'}
System.fields = {
    {'os', 'System'}, {'osVersion', 'Version'}, {'deviceKind', 'Kind of device'}, {'deviceModel', 'Model'}, {'manufacturer', 'Manufacturer'},
    {'cpuName', 'Processor'}, {'cpuCores', 'Processor cores'}, {'gpuName', 'Graphics'}, {'memoryBytes', 'Memory'},
    {'languages', 'Preferred languages'}, {'timeZone', 'Time zone'}, {'theme', 'Theme'}, {'battery', 'Battery'}, {'powerSaving', 'Power saving'},
}
System.kept = 20
System.longest = 44

-- The states of the battery in words, without the ones the platform keeps to itself.
System.batteryStates = {charging = 'charging', discharging = 'on battery power', full = 'full', none = 'no battery'}

local function describeBattery(battery)
    local state = System.batteryStates[battery.state]
    if not state then
        return 'Not reported'
    end
    if not battery.level then
        return state:sub(1, 1):upper() .. state:sub(2)
    end
    return string.format('%.0f percent, %s', battery.level * 100, state)
end

-- Whether the power saving mode of the system is on, which some platforms do not tell.
local function describePowerSaving(enabled)
    if enabled == nil then
        return 'Not reported'
    end
    return enabled and 'On' or 'Off'
end

function System.describeEvent(name, data)
    if name == 'batteryChanged' then
        return describeBattery(data)
    elseif name == 'powerSavingChanged' then
        return 'Power saving is now ' .. describePowerSaving(data.enabled):lower() .. '.'
    elseif name == 'timeZoneChanged' then
        return 'The time zone is now "' .. data.timeZone .. '".'
    elseif name == 'systemLanguagesChanged' then
        return 'The device now prefers "' .. table.concat(data.languages, '", "') .. '".'
    end
    return 'The theme is now "' .. tostring(data.theme) .. '".'
end

-- Shortens a long name, such as the renderer string of a browser, to what fits a row, and keeps the whole of it for the tooltip.
local function shortened(text)
    local renderer = text:match('^ANGLE %(.-,%s*(.-)%s*,') or text
    if #renderer > System.longest then
        return renderer:sub(1, System.longest - 1) .. '…'
    end
    return renderer
end

function System:enter()
    self.device = system.info()
    self.lines = {}
    self.logged = 0
    for _, name in ipairs(System.events) do
        self:listen(name, function(data)
            self:record(string.format('Event "%s"', name), System.describeEvent(name, data))
        end)
    end
    local rows = {}
    for index, field in ipairs(System.fields) do
        rows[index] = ui.row{gap = 16, alignItems = 'start',
            ui.label{text = field[2], color = 'textMuted', width = 300},
            ui.label{id = 'value-' .. field[1], text = '', grow = 1},
        }
    end
    self:frame{
        hint = 'Switch the system between light and dark, plug the charger in, turn power saving on, or change the time zone or the languages of the device, and the change arrives as an event.',
        focus = 'copy',
        controls = {
            ui.button{id = 'copy', text = 'Copy the device', variant = 'primary', onClick = function() self:copy() end},
            ui.button{id = 'paste', text = 'Read the clipboard', onClick = function() self:paste() end},
            ui.label{text = haylen.platform == 'android' and 'On an emulator, "adb shell cmd uimode night yes" switches the theme, and "adb shell dumpsys battery set level 37" changes the battery.' or 'A browser copies and reads only shortly after a click or a key, and some platforms ask the person before an app reads the clipboard.', color = 'textMuted', font = 'caption'},
        },
        content = {ui.row{grow = 1, gap = 24, align = 'stretch',
            ui.panel{grow = 1, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'What the system tells'},
                ui.scroll{grow = 1, height = 0, ui.column{gap = 10, children = rows}},
            },
            ui.panel{width = 600, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Events and answers'},
                ui.column{id = 'eventsEmpty', grow = 1, gap = 16, justify = 'center', align = 'stretch',
                    ui.icon{image = 'plugins/bell.svg', size = 96, color = 'textMuted', align = 'center'},
                    ui.label{text = 'Nothing yet. Copy the device, or switch the theme of the system, to see the answer arrive.', color = 'textMuted', textAlign = 'center', align = 'center', maxWidth = 440},
                },
                ui.scroll{id = 'eventsScroll', grow = 1, height = 0, focusable = false, visible = false, ui.list{id = 'events', items = {}}},
            },
        }},
        code = "local info = system.info()\nprint(info.os, info.deviceModel, info.cpuCores)\nprint(system.theme(), system.battery().level, system.powerSaving(), system.timeZone())\nprint(table.concat(system.languages(), ', '))\nlocal _, message = system.setClipboard(info.deviceModel):await()\nlocal text = system.clipboard():await()",
    }
    self:showValues()
end

function System:record(text, detail)
    self.logged = self.logged + 1
    table.insert(self.lines, 1, {id = 'line-' .. self.logged, text = text, caption = os.date('%H:%M:%S') .. '  ' .. detail})
    self.lines[System.kept + 1] = nil
    self:set('events', {items = self.lines})
    self:set('eventsEmpty', {visible = false})
    self:set('eventsScroll', {visible = true})
end

-- Copies the model and the system of the device in a task of the scene, which waits for the answer and ends with the scene.
function System:copy()
    local summary = string.format('%s, %s', self.device.deviceModel or 'Unknown model', self.device.os)
    local frame = haylen.frameIndex()
    self:record('Copying the device', '"' .. summary .. '"')
    self:spawn(function()
        local _, message, failure = system.setClipboard(summary):await()
        local frames = haylen.frameIndex() - frame
        if failure then
            self:record(string.format('The copy failed with "%s"', failure.code), string.format('%s It answered after %d frames.', message, frames))
            return
        end
        self:record('The clipboard took the text', string.format('It answered after %d frames.', frames))
    end)
end

-- A platform that asks the person before an app reads the clipboard answers "denied" with the sentence of its own failure.
function System:paste()
    self:spawn(function()
        local text, message, failure = system.clipboard():await()
        if failure then
            self:record(string.format('The read failed with "%s"', failure.code), message)
            return
        end
        if #text == 0 then
            self:record('The clipboard was read', 'It holds no text.')
            return
        end
        self:record('The clipboard was read', string.format('It holds %d bytes of text.', #text))
    end)
end

-- The value of each field for people, with the fields the platform does not report marked so.
function System:values()
    local device = self.device
    local values = {}
    for _, field in ipairs(System.fields) do
        local value = device[field[1]]
        values[field[1]] = value ~= nil and tostring(value) or 'Not reported'
    end
    values.gpuName = device.gpuName and shortened(device.gpuName) or 'Not reported'
    values.memoryBytes = device.memoryBytes and string.format('%.1f GB', device.memoryBytes / 2 ^ 30) or 'Not reported'
    local languages = system.languages()
    values.languages = #languages > 0 and table.concat(languages, ', ') or 'Not reported'
    values.theme = system.theme()
    values.battery = describeBattery(system.battery())
    values.timeZone = system.timeZone() or 'Not reported'
    values.powerSaving = describePowerSaving(system.powerSaving())
    return values
end

function System:showValues()
    self.shown = self.shown or {}
    for key, value in pairs(self:values()) do
        if self.shown[key] ~= value then
            self.shown[key] = value
            self:set('value-' .. key, {text = value})
        end
    end
    if self.device.gpuName and not self.gpuTooltip then
        self.gpuTooltip = true
        self:set('value-gpuName', {tooltip = self.device.gpuName})
    end
end

function System:update(dt)
    System.super.update(self, dt)
    self:showValues()
end

return System
