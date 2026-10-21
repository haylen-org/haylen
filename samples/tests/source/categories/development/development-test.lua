-- The base of the hot reload tests: a test names the file to edit and the steps to follow, and the status line shows whether hot reload is on, its mode, how many modules reloaded and the last one. A test draws in its play area, or gives the nodes of its `content` in place of it. Hot reload is on while the project runs in development, with "haylen run samples/tests" or with "--platform web", and the tests say so when it is off.
local events = require('haylen.events')
local haylen = require('haylen')
local hotReload = require('haylen.hotReload')
local ui = require('haylen.ui')

local Test = require('harness.test')

local DevelopmentTest = haylen.class('DevelopmentTest', Test)

DevelopmentTest.file = ''
DevelopmentTest.steps = {}
DevelopmentTest.off = 'Hot reload is off, because the project does not run in development. Run it with "haylen run samples/tests" or with "--platform web" to try it.'

function DevelopmentTest:init(entry)
    DevelopmentTest.super.init(self, entry)
    self.reloads = 0
    self.last = nil
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
    self:frame{code = self.code, hint = hotReload.active() and 'Edit the file, save it and watch the test change while it runs.' or DevelopmentTest.off, controls = controls, content = self:content(), focus = self.firstControl}
    events.on('moduleReloaded', function(event)
        self.reloads = self.reloads + 1
        self.last = event.path
    end, {owner = self})
end

function DevelopmentTest:controls()
    return {}
end

-- The nodes that take the place of the play area, or nothing for a test that draws in it.
function DevelopmentTest:content()
    return nil
end

-- A card of the content with a caption over a value, whose label takes the id `id`.
function DevelopmentTest.valueCard(id, caption, value)
    return ui.card{grow = 1, gap = 8, align = 'stretch',
        ui.label{text = caption, font = 'caption', color = 'textMuted'},
        ui.label{id = id, text = value, font = 'heading'},
    }
end

function DevelopmentTest:update(dt)
    DevelopmentTest.super.update(self, dt)
    local last = self.last and string.format(', the last "%s"', self.last) or ''
    self:status(string.format('Hot reload is %s in the mode "%s". Modules reloaded %d%s.', hotReload.active() and 'on' or 'off', hotReload.mode(), self.reloads, last))
end

return DevelopmentTest
