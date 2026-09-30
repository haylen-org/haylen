-- System: what haylen.system tells about the device, its theme and its battery with their changes as they happen, and an address to open and a vibration to play, with the answer of the url and the frames it took.
local haylen = require('haylen')
local system = require('haylen.system')
local ui = require('haylen.ui')

local Journal = require('journal')
local sample = require('sample')

local System = haylen.class('System', sample.Test)

local kEvents = {systemThemeChanged = sample.violet, batteryChanged = sample.green}
local kFields = {'os', 'osVersion', 'deviceKind', 'deviceModel', 'manufacturer', 'cpuName', 'cpuCores', 'gpuName', 'locale', 'timeZone'}

local function describeBattery(battery)
    local level = battery.level and string.format('%.0f%%', battery.level * 100) or 'unknown'
    return string.format('State "%s", level %s, %s', battery.state, level, battery.charging and 'charging' or 'not charging')
end

function System:enter()
    self.device = system.info()
    self.journal = Journal(16)
    for name, color in pairs(kEvents) do
        self:listen(name, function(data)
            self.journal:add(string.format('Event "%s": %s', name, sample.json(data)), color)
        end)
    end
    self:frame({
        hint = 'Switch the system between light and dark or plug the charger in, and the change arrives as an event.',
        focus = 'open',
        controls = {
            ui.button{id = 'open', text = 'Open "example.com"', variant = 'primary', onClick = function() self:open('https://example.com') end},
            ui.button{id = 'openNothing', text = 'Open an address no app takes', onClick = function() self:open('haylen-sample-nothing://open') end},
            ui.button{id = 'vibrate', text = 'Vibrate for 80 ms', onClick = function() self:vibrate() end},
            ui.label{text = 'Android phones and phone browsers vibrate, iPhones play a haptic impact and the other devices do nothing. Values a platform does not report are "nil".', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "local info = system.info()\nsystem.theme()\nsystem.battery()\nsystem.openUrl(url):await()"},
        },
    })
end

-- Opens a url in a task of the scene, which waits for the answer and ends with the scene.
function System:open(url)
    local frame = haylen.frameIndex()
    self.journal:add(string.format('Called "system.openUrl" with "%s".', url), sample.accent)
    self:spawn(function()
        local opened = system.openUrl(url):await()
        local text = string.format('The address "%s" answered after %d frames: %s.', url, haylen.frameIndex() - frame, opened and 'an app took it' or 'no app took it')
        self.journal:add(text, opened and sample.green or sample.red)
    end)
end

function System:vibrate()
    system.vibrate(0.08)
    self.journal:add('Called "system.vibrate(0.08)".', sample.accent)
end

function System:rows()
    local rows = {}
    for _, field in ipairs(kFields) do
        local value = self.device[field]
        rows[#rows + 1] = {'"' .. field .. '"', value ~= nil and tostring(value) or 'Not reported'}
    end
    local memory = self.device.memoryBytes
    rows[#rows + 1] = {'"memoryBytes"', memory and string.format('%.1f GB', memory / 2 ^ 30) or 'Not reported'}
    rows[#rows + 1] = {'"languages"', #self.device.languages > 0 and table.concat(self.device.languages, ', ') or 'Not reported'}
    rows[#rows + 1] = {'"system.theme()"', '"' .. system.theme() .. '"'}
    rows[#rows + 1] = {'"system.battery()"', describeBattery(system.battery())}
    return rows
end

function System:update(dt)
    System.super.update(self, dt)
    local os = self.device.osVersion and self.device.os .. ' ' .. self.device.osVersion or self.device.os
    self:status(string.format('System "%s" on a "%s"   theme "%s"   battery "%s"', os, self.device.deviceKind, system.theme(), system.battery().state))
end

function System:draw(area)
    local y = 20
    for _, row in ipairs(self:rows()) do
        sample.caption(row[1], 24, y, {size = 20})
        sample.caption(row[2], 300, y, {size = 20, color = sample.ink, maxWidth = area.width - 324})
        y = y + 30
    end
    sample.caption('Events and answers', 24, y + 20, {size = 22, color = sample.ink})
    self.journal:draw(24, y + 56, area.height - y - 76, 19)
end

return System
