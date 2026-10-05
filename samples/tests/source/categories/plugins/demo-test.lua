-- The base of the tests of the category: the rows of results on the stage, the summary in the status line and the native part of the plugin `native-demo`. Without the native part a test shows why and keeps only its explanation.
local async = require('async')
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')

local Results = require('harness.results')
local Test = require('harness.test')
local demo = require('native-demo')

local DemoTest = haylen.class('DemoTest', Test)

DemoTest.missing = 'The native part is not available on this platform.'

function DemoTest:init(entry)
    DemoTest.super.init(self, entry)
    self.results = Results(entry.code)
end

function DemoTest:frame(options)
    self.native = demo.available()
    if not self.native then
        self.results:set('native', 'skip', 'Native part', DemoTest.missing)
        options.focus = 'back'
    end
    DemoTest.super.frame(self, options)
end

function DemoTest:update(dt)
    DemoTest.super.update(self, dt)
    self:status(string.format('%s, platform "%s", native "%s", %d pending calls', self.results:summary(), haylen.platform, tostring(demo.available()), platform.pendingCallCount()))
end

-- Draws the rows of results. Tests that draw more call it first.
function DemoTest:draw(area)
    self.results:draw(area)
end

-- Runs `body` as a task of the test when the native part runs here, and otherwise only shows that it is missing.
function DemoTest:act(body)
    if self.native then
        self:spawn(body)
    end
end

-- Writes a value from the bridge as JSON, the form it crossed the bridge in.
function DemoTest.json(value)
    if value == nil then
        return 'null'
    end
    return json.encode(value)
end

-- Waits inside a task until `condition` holds, checking every frame for at most `seconds` of app time, and returns whether it held.
function DemoTest.waitFor(condition, seconds)
    local deadline = haylen.elapsed() + seconds
    while not condition() and haylen.elapsed() < deadline do
        async.sleep(10):await()
    end
    return condition()
end

return DemoTest
