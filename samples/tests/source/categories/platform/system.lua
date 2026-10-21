-- What `haylen.system` tells about the device, its theme and its battery with their changes as they happen, and an address to open and a vibration to play, with the answer of the address and the frames it took.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Test = require('harness.test')

local System = haylen.class('System', Test)

System.events = {'systemThemeChanged', 'batteryChanged'}
System.fields = {
    {'os', 'System'}, {'osVersion', 'Version'}, {'deviceKind', 'Kind of device'}, {'deviceModel', 'Model'}, {'manufacturer', 'Manufacturer'},
    {'cpuName', 'Processor'}, {'cpuCores', 'Processor cores'}, {'gpuName', 'Graphics'}, {'memoryBytes', 'Memory'}, {'locale', 'Language'},
    {'languages', 'Preferred languages'}, {'timeZone', 'Time zone'}, {'theme', 'Theme'}, {'battery', 'Battery'},
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
            self:record(string.format('Event "%s"', name), name == 'batteryChanged' and describeBattery(data) or 'The theme is now "' .. tostring(data.theme) .. '".')
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
        hint = 'Switch the system between light and dark or plug the charger in, and the change arrives as an event.',
        focus = 'open',
        controls = {
            ui.button{id = 'open', text = 'Open "example.com"', variant = 'primary', onClick = function() self:open('https://example.com') end},
            ui.button{id = 'openNothing', text = 'Open an address no app takes', onClick = function() self:open('haylen-tests-nothing://open') end},
            ui.button{id = 'vibrate', text = 'Vibrate for 80 ms', onClick = function() self:vibrate() end},
            ui.label{text = haylen.platform == 'android' and 'On an emulator, "adb shell cmd uimode night yes" switches the theme, and "adb shell dumpsys battery set level 37" changes the battery.' or 'Android phones and phone browsers vibrate, iPhones play a haptic impact and the other devices do nothing.', color = 'textMuted', font = 'caption'},
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
                    ui.label{text = 'Nothing yet. Open an address, or switch the theme of the system, to see the answer arrive.', color = 'textMuted', textAlign = 'center', align = 'center', maxWidth = 440},
                },
                ui.scroll{id = 'eventsScroll', grow = 1, height = 0, focusable = false, visible = false, ui.list{id = 'events', items = {}}},
            },
        }},
        code = "local info = system.info()\nprint(info.os, info.deviceModel, info.cpuCores)\nprint(system.theme(), system.battery().level)\nlocal opened = system.openUrl('https://example.com'):await()\nsystem.vibrate(0.08)",
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

-- Opens an address in a task of the scene, which waits for the answer and ends with the scene.
function System:open(url)
    local frame = haylen.frameIndex()
    self:record('Opening an address', '"' .. url .. '"')
    self:spawn(function()
        local opened = system.openUrl(url):await()
        self:record(opened and 'An app took the address' or 'No app took the address', string.format('"%s" answered after %d frames.', url, haylen.frameIndex() - frame))
    end)
end

function System:vibrate()
    system.vibrate(0.08)
    self:record('Vibrating', 'The call "system.vibrate(0.08)" asked for 80 ms.')
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
    values.languages = #device.languages > 0 and table.concat(device.languages, ', ') or 'Not reported'
    values.theme = system.theme()
    values.battery = describeBattery(system.battery())
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
