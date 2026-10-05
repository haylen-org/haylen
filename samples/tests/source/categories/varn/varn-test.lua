-- The base of the Varn tests: a test runs its lesson in a task of its scene, shows the Lua code of the lesson in the panel and draws its checks as rows on the stage. Run again repeats the lesson.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Results = require('harness.results')
local Test = require('harness.test')

local VarnTest = haylen.class('VarnTest', Test)

VarnTest.panelWidth = 720
VarnTest.hint = 'Run again repeats the lesson.'

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
        controls[#controls + 1] = ui.card{ui.label{text = excerpt[2], font = 'monospace'}}
    end
    self:frame{hint = self.hint, focus = 'run', panelWidth = VarnTest.panelWidth, controls = controls}
    self:start()
end

-- Runs the lesson in a task of the scene, which stops for good when the test exits, and one run at a time.
function VarnTest:start()
    if self.running then
        return
    end
    self.running = true
    self.results:clear()
    self:set('run', {text = 'Running'})
    self:spawn(function()
        self:run()
        self.running = false
        self:set('run', {text = 'Run again'})
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

function VarnTest:draw(area)
    self.results:draw(area)
end

-- Returns a value for a to-be-closed variable that closes `resource` when the variable leaves its scope, which also happens when the test exits while its task waits.
function VarnTest.closing(resource)
    return setmetatable({}, {__close = function()
        resource:close()
    end})
end

return VarnTest
