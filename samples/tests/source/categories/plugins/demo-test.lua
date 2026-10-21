-- The base of the tests of the category: the checks in a panel next to what the test shows, the state of the native part of the plugin `native-demo` in the status line, and the calls that wait for it. Without the native part a test shows why and keeps only its explanation.
local async = require('async')
local haylen = require('haylen')
local json = require('json')
local platform = require('haylen.platform')
local ui = require('haylen.ui')

local Results = require('harness.results')
local Test = require('harness.test')
local demo = require('native-demo')

local DemoTest = haylen.class('DemoTest', Test)

DemoTest.missing = 'The native part is not available on this platform.'
DemoTest.checksWidth = 620

function DemoTest:init(entry)
    DemoTest.super.init(self, entry)
    self.results = Results(entry.code)
end

-- Mounts the frame with the checks as its content, or beside the nodes of `showcase`, which show what the test does, such as a video, a store or an account. The options `checksTitle` and `checksEmpty` name the panel of the checks and tell what to do while it is empty.
function DemoTest:frame(options)
    self.native = demo.available()
    if not self.native then
        self.results:set('native', 'skip', 'Native part', DemoTest.missing)
        options.focus = 'back'
    end
    local checks = self.results:view(options.checksTitle, {grow = options.showcase and 0 or 1, width = options.showcase and DemoTest.checksWidth, empty = options.checksEmpty})
    local content = {checks}
    if options.showcase then
        content = {ui.row{grow = 1, gap = 24, align = 'stretch', ui.column{grow = 1, gap = 16, align = 'stretch', children = options.showcase}, checks}}
    end
    DemoTest.super.frame(self, {hint = options.hint, focus = options.focus, controls = options.controls, code = options.code, content = content})
    self.results:show(self.gui)
end

function DemoTest:update(dt)
    DemoTest.super.update(self, dt)
    local pending = platform.pendingCallCount()
    local native = demo.available() and 'The native part of "native-demo" runs here' or 'The native part of "native-demo" does not run here'
    local calls = pending == 0 and 'no call waits for an answer' or string.format('%d %s for an answer', pending, pending == 1 and 'call waits' or 'calls wait')
    self:status(string.format('%s, and %s.', native, calls))
end

-- A frame that waits for what a test will show in it, such as a video or a map, with an icon and a line that says what comes there.
function DemoTest.placeholder(id, icon, text)
    return ui.column{id = id, align = 'center', gap = 16,
        ui.icon{image = icon, size = 96, color = 'textMuted', align = 'center'},
        ui.label{id = id .. '-text', text = text, color = 'textMuted', textAlign = 'center', maxWidth = 520},
    }
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
