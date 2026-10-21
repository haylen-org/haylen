-- The base of the Varn tests: a test runs its lesson in a task of its scene, shows the Lua code of the lesson in the panel and its checks as cards, with the nodes its `showcase` returns under them, or beside them when it sets `showcaseWidth`, and a drawing of what the lesson does on a stage under them when the test sets `stageHeight`, or beside them when it sets `stageWidth`. Run again repeats the lesson.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Results = require('harness.results')
local Test = require('harness.test')
local code = require('harness.code')

local VarnTest = haylen.class('VarnTest', Test)

VarnTest.panelWidth = 720
VarnTest.hint = 'Run again repeats the lesson.'
VarnTest.stageRadius = 16

-- The code of a test, as a list of `{title, code}` excerpts that the panel shows in order.
VarnTest.excerpts = {}

function VarnTest:init(entry)
    VarnTest.super.init(self, entry)
    self.results = Results(entry.code)
end

function VarnTest:enter()
    local controls = {ui.button{id = 'run', text = 'Run again', variant = 'primary', onClick = function() self:start() end}}
    for _, excerpt in ipairs(self.excerpts) do
        controls[#controls + 1] = ui.sectionTitle{text = excerpt[1]}
        controls[#controls + 1] = ui.card{align = 'stretch', ui.richText{text = code.markup(excerpt[2]), font = 'monospace'}}
    end
    local content = {self.results:view('Checks')}
    if self.showcaseWidth then
        content = {ui.row{grow = 1, gap = 16, align = 'stretch', content[1], ui.column{width = self.showcaseWidth, gap = 16, align = 'stretch', children = self:showcase()}}}
    else
        for _, node in ipairs(self:showcase()) do
            content[#content + 1] = node
        end
    end
    if self.stageHeight then
        content[#content + 1] = ui.playArea{id = 'stage', height = self.stageHeight, align = 'stretch', focusable = false}
    elseif self.stageWidth then
        content = {ui.row{grow = 1, gap = 16, align = 'stretch', ui.column{grow = 1, gap = 16, align = 'stretch', children = content}, ui.playArea{id = 'stage', width = self.stageWidth, align = 'stretch', focusable = false}}}
    end
    self:frame{hint = self.hint, focus = 'run', panelWidth = VarnTest.panelWidth, controls = controls, content = content}
    self.results:show(self.gui)
    self:start()
end

-- The nodes a test shows under its checks, such as a clock.
function VarnTest:showcase()
    return {}
end

-- Runs the lesson in a task of the scene, which stops for good when the test exits, and one run at a time.
function VarnTest:start()
    if self.running then
        return
    end
    self.running = true
    self.results:clear()
    self:set('run', {text = 'Running…', enabled = false})
    self:spawn(function()
        self:run()
        self.running = false
        self:set('run', {text = 'Run again', enabled = true})
    end)
end

function VarnTest:run()
end

function VarnTest:check(key, name, passed, detail)
    self.results:set(key, passed and 'pass' or 'fail', name, detail)
end

function VarnTest:update(dt)
    VarnTest.super.update(self, dt)
    self:status(self.results:summary())
end

-- The stage of a test draws on a card of its own, rounded like the panels, instead of the square stage of the harness.
function VarnTest:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera)
    graphics2d.drawShape({0, 0, self.area.width, self.area.height}, {radius = VarnTest.stageRadius, color = Test.surface, borderWidth = 2, borderColor = Test.line, layer = -100})
    self:draw(self.area)
end

-- Returns a value for a to-be-closed variable that closes `resource` when the variable leaves its scope, which also happens when the test exits while its task waits.
function VarnTest.closing(resource)
    return setmetatable({}, {__close = function()
        resource:close()
    end})
end

return VarnTest
