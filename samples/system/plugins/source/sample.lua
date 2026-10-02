-- What every test shares: the frame with the Back button, the title and the description, the stage with the rows of results, a panel of controls on the right, a status line and a line of hints, and the rows themselves. Escape, the east gamepad button and the Menu button of a TV remote reach the root of the document as a cancel, which goes back to the menu.
local async = require('async')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local json = require('json')
local m = require('haylen.math')
local platform = require('haylen.platform')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local demo = require('native-demo')

local sample = {}

local kTransition = {effect = 'fade', duration = 0.3, color = '#FF101418'}
local kPanelWidth = 520
local kStageColor = '#FF181D26'
local kStatusInterval = 0.2

sample.ink = '#FFE8EAF2'
sample.muted = '#FF7A8099'
sample.green = '#FF6FDCA0'
sample.red = '#FFFF8A84'
sample.warm = '#FFF2B23A'
sample.accent = '#FF8FB0FF'

sample.missing = 'The native part is not available on this platform.'

-- A row of results: a state, a name and a detail. The states are waiting, pass, fail, info and skip, which marks what does not apply to the platform, such as an unsupported call.
local Results = haylen.class('Results')
sample.Results = Results

local kColors = {waiting = sample.warm, pass = sample.green, fail = sample.red, info = sample.accent, skip = sample.muted}
local kMarks = {waiting = '...', pass = 'PASS', fail = 'FAIL', info = 'INFO', skip = 'N/A'}

function Results:init(test)
    self.test = test
    self.rows = {}
    self.byKey = {}
end

-- Adds the row of `key`, or changes it, and prints it to the log whenever it settles in a new state.
function Results:set(key, state, name, detail)
    local row = self.byKey[key]
    if not row then
        row = {}
        self.byKey[key] = row
        self.rows[#self.rows + 1] = row
    end
    local settled = state ~= 'waiting' and row.state ~= state
    row.state, row.name, row.detail = state, name, tostring(detail or '')
    if settled then
        print(string.format('%s in the plugins test "%s": %s. %s', kMarks[state], self.test, name, row.detail))
    end
end

function Results:clear()
    self.rows = {}
    self.byKey = {}
end

-- Sets the row from a failed call: an unsupported call does not apply to the platform, and any other failure fails the row.
function Results:failure(key, name, err)
    if err.code == 'unsupported' then
        self:set(key, 'skip', name, 'Unsupported on this platform: ' .. err.message)
    else
        self:set(key, 'fail', name, string.format('%s (code "%s")', err.message, tostring(err.code)))
    end
end

function Results:summary()
    local counts = {waiting = 0, pass = 0, fail = 0, info = 0, skip = 0}
    for _, row in ipairs(self.rows) do
        counts[row.state] = counts[row.state] + 1
    end
    return string.format('%d passed   %d failed   %d not applicable   %d waiting', counts.pass, counts.fail, counts.skip, counts.waiting)
end

function Results:draw(area)
    local width = area.width - 150
    local y = 24
    for _, row in ipairs(self.rows) do
        sample.caption(kMarks[row.state], 24, y, {size = 24, color = kColors[row.state]})
        sample.caption(row.name, 120, y, {size = 24, color = sample.ink})
        sample.caption(row.detail, 120, y + 32, {size = 19, maxWidth = width})
        local _, height = graphics2d.measureText(nil, row.detail, {size = 19, maxWidth = width})
        y = y + 32 + height + 24
    end
end

local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

-- The stage camera shows stage-local coordinates: 0, 0 is the top-left corner of the stage wherever the layout puts it.
function Test:init(info)
    self.info = info
    self.statusTime = kStatusInterval
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
    self.results = Results(info.id)
end

-- Mounts the frame. `options` holds the `hint`, the `controls` of the panel and the id of the control to `focus` for gamepads and TV remotes. Without the native part, the test shows why and keeps only its explanation.
function Test:frame(options)
    window.setBackLeavesApp(false)
    self.native = demo.available()
    if not self.native then
        self.results:set('native', 'skip', 'Native part', sample.missing)
    end
    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 12,
        onCancel = sample.back,
        ui.row{gap = 24, align = 'start',
            ui.button{id = 'back', text = 'Back', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.info.title, font = 'heading'},
                ui.label{text = self.info.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, gap = 16,
            ui.spacer{id = 'stage', grow = 1, align = 'stretch'},
            ui.panel{id = 'panel', width = kPanelWidth, align = 'stretch', ui.scroll{grow = 1, height = 0, ui.column{gap = 12, children = options.controls}}},
        },
        ui.label{id = 'status', text = '', font = 'monospace', color = 'accentText'},
        ui.label{text = (options.hint and options.hint .. ' ' or '') .. 'Escape, the east button or Back returns to the menu.', font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(self.native and options.focus or 'back', 'focus')
end

-- Follows the stage, calling `resize` with the stage-local area whenever its size changes. Tests call it first from their own update.
function Test:update(dt)
    self.statusTime = self.statusTime + haylen.unscaledDelta()

    local stage = self.document:bounds('stage')
    if stage then
        self.stage = stage
        self.camera.viewport = stage
        if not self.area or self.area.width ~= stage.width or self.area.height ~= stage.height then
            self.area = m.rect(0, 0, stage.width, stage.height)
        end
    end
    self:status(self.results:summary() .. '   platform ' .. haylen.platform .. '   native ' .. tostring(demo.available()) .. '   pending calls ' .. platform.pendingCallCount())
end

-- Draws the stage background with the rows of results, and lets the test draw more in stage-local coordinates.
function Test:render()
    if self.area then
        graphics2d.beginWorld(self.camera)
        graphics2d.drawRect(self.area, kStageColor, {layer = -100})
        self.results:draw(self.area)
        self:draw(self.area)
    end
end

function Test:draw(area)
end

-- Shows a line of live values a few times per second, so the numbers stay readable.
function Test:status(text)
    if self.statusTime >= kStatusInterval then
        self.statusTime = 0
        self.document:set('status', {text = text})
    end
end

function Test:set(id, properties)
    self.document:set(id, properties)
end

-- Runs `body` as a task of the test when the native part runs here, and otherwise only shows that it is missing.
function Test:act(body)
    if self.native then
        self:spawn(body)
    end
end

-- Writes a value from the bridge as JSON, the form it crossed the bridge in.
function sample.json(value)
    if value == nil then
        return 'null'
    end
    return json.encode(value)
end

-- Waits inside a task until `condition` holds, checking every frame for at most `seconds` of app time, and returns whether it held.
function sample.waitFor(condition, seconds)
    local deadline = haylen.elapsed() + seconds
    while not condition() and haylen.elapsed() < deadline do
        async.sleep(10):await()
    end
    return condition()
end

-- Draws a small caption, the way every test labels what it draws.
function sample.caption(text, x, y, options)
    options = options or {}
    graphics2d.drawText(nil, text, x, y, {size = options.size or 22, color = options.color or sample.muted, anchor = options.anchor or {0, 0}, maxWidth = options.maxWidth or 0, layer = options.layer or 2})
end

function sample.open(info)
    if not scene.transitioning() then
        scene.push(require(info.module)(info), kTransition)
    end
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(kTransition)
    end
end

return sample
