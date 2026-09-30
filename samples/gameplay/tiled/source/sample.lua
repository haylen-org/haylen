-- The frame every test shares: a header with the Back button, the title and the description, a panel of options, a hint line and the stage the test draws in.
local haylen = require('haylen')
local graphics2d = require('haylen.graphics2d')
local m = require('haylen.math')
local profiler = require('haylen.debug')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Pointer = require('pointer')

local sample = {}

local kMargin = 24
local kGap = 12
local kHeaderHeight = 112
local kHintHeight = 64
local kPanelWidth = 400
local kStageColor = '#FF1A2029'
local kStatsInterval = 0.25
local kTransition = {effect = 'fade', duration = 0.3}

local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

function Test:init(info)
    self.info = info
    self.camera = graphics2d.newCamera()
    self.pointer = Pointer.new()
    self.statsTime = 0
    self.timings = {}
    self.zoomScale = 1
end

-- Mounts the frame. The table `options` holds the `hint`, the `controls` of the panel, whether the panel shows `stats`, the world `view` the camera fits into the stage and the control to `focus` for gamepads and TV remotes.
function Test:enter(options)
    self.viewSize = options.view or {1600, 860}
    self.hasPanel = options.controls ~= nil or options.stats == true

    local middle = {ui.spacer{grow = 1}}
    if self.hasPanel then
        local controls = {}
        for _, control in ipairs(options.controls or {}) do
            controls[#controls + 1] = control
        end
        if options.stats then
            controls[#controls + 1] = ui.label{id = 'stats', text = '', font = 'monospace'}
        end
        middle[2] = ui.panel{width = kPanelWidth, align = 'start', gap = kGap, children = controls}
    end

    -- The Menu button of a TV remote and the Back button of Android reach the test as Escape instead of leaving the app.
    window.setBackLeavesApp(false)
    self.document = ui.mount(ui.column{
        padding = kMargin,
        gap = kGap,
        onCancel = sample.back,
        ui.row{height = kHeaderHeight, gap = 24, align = 'stretch',
            ui.button{id = 'back', text = 'Back', align = 'start', onClick = sample.back},
            ui.column{grow = 1, gap = 4, align = 'start',
                ui.label{text = self.info.title, font = 'heading'},
                ui.label{text = self.info.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, align = 'stretch', children = middle},
        ui.label{id = 'hint', height = kHintHeight, text = options.hint or '', font = 'caption', color = 'textMuted'},
    })
    if options.focus then
        self.document:command(options.focus, 'focus')
    end
    self:layout()
end

function Test:exit()
    self.document:unmount()
end

-- Keeps the stage and the pointer current. Tests call it first from their own update.
function Test:update(dt)
    self:layout()
    self.pointer:update(dt, self.stage, self.camera)
    self.statsTime = self.statsTime + dt
end

-- The stage is the part of the safe area the frame leaves free, and the camera shows the view of the test inside it, zoomed further by the zoom scale of the test.
function Test:layout()
    local safe = viewport.safeRect()
    local top = safe.y + kMargin + kHeaderHeight + kGap
    local right = safe:right() - kMargin - (self.hasPanel and kPanelWidth + kGap or 0)
    local bottom = safe:bottom() - kMargin - kHintHeight - kGap
    self.stage = m.rect(safe.x + kMargin, top, right - safe.x - kMargin, bottom - top)
    self.camera.viewport = self.stage
    local zoom = math.min(self.stage.width / self.viewSize[1], self.stage.height / self.viewSize[2]) * self.zoomScale
    self.camera.zoom = {zoom, zoom}
end

-- Begins the world canvas of the test with the stage filled behind it.
function Test:beginWorld(options)
    graphics2d.beginWorld(self.camera, options)
    graphics2d.drawRect(graphics2d.canvasBounds(), kStageColor, {layer = -1000})
end

function Test:renderUi()
    graphics2d.beginScreen()
    self.pointer:draw()
end

function Test:set(id, properties)
    self.document:set(id, properties)
end

-- Returns the milliseconds of the last frame that ran the profiler scope `name`, and keeps it until the scope runs again. Tests call it every frame, because the profiler only holds the last frame.
function Test:timing(name)
    local milliseconds = sample.milliseconds(name)
    if milliseconds > 0 then
        self.timings[name] = milliseconds
    end
    return self.timings[name] or 0
end

-- Shows the lines of `text` under the controls, a few times per second so the numbers stay readable.
function Test:showStats(text)
    if self.statsTime >= kStatsInterval then
        self.statsTime = 0
        self.document:set('stats', {text = text})
    end
end

function sample.open(info)
    scene.push(require(info.module)(info), kTransition)
end

function sample.back()
    if not scene.transitioning() then
        scene.pop(kTransition)
    end
end

-- Returns the milliseconds the profiler scope `name` took in the last frame, or 0 when it did not run.
function sample.milliseconds(name)
    for _, scope in ipairs(profiler.frame().scopes) do
        if scope.name == name then
            return scope.milliseconds
        end
    end
    return 0
end

return sample
