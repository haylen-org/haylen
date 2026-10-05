-- The base of the hot reload tests: a test names the file to edit and the steps to follow, and the status line shows whether hot reload is on, its mode, how many modules reloaded and the last one. Hot reload is on while the project runs in development, with "python3 haylen.py run samples/tests" or with "--platform web", and the tests say so when it is off.
local events = require('haylen.events')
local haylen = require('haylen')
local hotReload = require('haylen.hotReload')
local ui = require('haylen.ui')

local Test = require('harness.test')

local DevelopmentTest = haylen.class('DevelopmentTest', Test)

DevelopmentTest.file = ''
DevelopmentTest.steps = {}
DevelopmentTest.off = 'Hot reload is off, because the project does not run in development. Run it with "python3 haylen.py run samples/tests" or with "--platform web" to try it.'

function DevelopmentTest:init(entry)
    DevelopmentTest.super.init(self, entry)
    self.reloads = 0
    self.last = 'none yet'
end

-- Mounts the frame with the file to edit and the steps in the panel, and counts the modules that reload.
function DevelopmentTest:enter()
    local controls = {ui.sectionTitle{text = 'File to edit'}, ui.card{ui.label{text = self.file, font = 'monospace'}}, ui.sectionTitle{text = 'Steps'}}
    for index, step in ipairs(self.steps) do
        controls[#controls + 1] = ui.label{text = string.format('%d. %s', index, step)}
    end
    for _, control in ipairs(self:controls()) do
        controls[#controls + 1] = control
    end
    self:frame{hint = hotReload.active() and 'Edit the file, save it and watch the test change while it runs.' or DevelopmentTest.off, controls = controls, focus = self.firstControl}
    events.on('moduleReloaded', function(event)
        self.reloads = self.reloads + 1
        self.last = event.path
    end, {owner = self})
end

function DevelopmentTest:controls()
    return {}
end

function DevelopmentTest:update(dt)
    DevelopmentTest.super.update(self, dt)
    self:status(string.format('Hot reload %s, mode "%s". Modules reloaded %d, the last "%s".', hotReload.active() and 'on' or 'off', hotReload.mode(), self.reloads, self.last))
end

return DevelopmentTest
