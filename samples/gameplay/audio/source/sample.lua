-- The frame every test shares: the Back button with the title and the description, the stage the test draws in, a panel of controls on the right, a status line and a line of hints. Every test preloads the sounds in its load hook, and Escape, the east gamepad button and the Menu button of a TV remote reach the root of the document as a cancel, which goes back only when no popup took it.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

local kTransition = {effect = 'fade', duration = 0.3, color = '#FF101418'}
local kPanelWidth = 480
local kStageColor = '#FF181D26'
local kStatusInterval = 0.2

sample.ink = '#FFE8EAF2'
sample.muted = '#FF7A8099'
sample.line = '#FF2C3147'
sample.surface = '#FF232938'
sample.idle = '#FF3A4258'
sample.accent = '#FF8FB0FF'
sample.warm = '#FFF2B23A'
sample.green = '#FF6FDCA0'
sample.red = '#FFFF8A84'
sample.violet = '#FFC9A0FF'

local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

-- The stage camera shows stage-local coordinates: 0, 0 is the top-left corner of the stage wherever the layout puts it.
function Test:init(info)
    self.info = info
    self.statusTime = kStatusInterval
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
end

-- The sounds decode in the background while the fade holds the covered screen.
function Test:load(context)
    context:preload('audio'):await()
end

-- Mounts the frame. `options` holds the `hint`, the `controls` of the panel, the id of the control to `focus` for gamepads and TV remotes and `overlay`, anchored nodes such as touch controls drawn over the stage.
function Test:frame(options)
    window.setBackLeavesApp(false)
    local children = {
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
    }
    for _, node in ipairs(options.overlay or {}) do
        children[#children + 1] = node
    end
    self.document = ui.mount(ui.column{padding = 24, gap = 12, onCancel = sample.back, children = children}, {owner = self})
    self.document:command(options.focus or 'back', 'focus')
end

-- Follows the stage, calling `resize` with the stage-local area whenever its size changes. Tests call it first from their own update.
function Test:update(dt)
    self.statusTime = self.statusTime + haylen.unscaledDelta()

    local stage = self.document:bounds('stage')
    if stage then
        self.camera.viewport = stage
        if not self.area or self.area.width ~= stage.width or self.area.height ~= stage.height then
            self.area = m.rect(0, 0, stage.width, stage.height)
            self:resize(self.area)
        end
    end
end

function Test:resize(area)
end

-- Draws the stage background and lets the test draw in stage-local coordinates once the stage has a size.
function Test:render()
    if self.area then
        graphics2d.beginWorld(self.camera)
        graphics2d.drawRect(self.area, kStageColor, {layer = -100})
        self:draw(self.area)
    end
end

function Test:draw(area)
end

-- Converts a point of the screen, such as the pointer, to stage-local coordinates.
function Test:toStage(x, y)
    return self.camera:screenToWorld(x, y)
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

-- Draws a small caption, the way every test labels what it draws.
function sample.caption(text, x, y, options)
    options = options or {}
    graphics2d.drawText(nil, text, x, y, {size = options.size or 22, color = options.color or sample.muted, anchor = options.anchor or {0, 0}, maxWidth = options.maxWidth or 0, layer = options.layer or 2})
end

-- Draws a horizontal bar filled to `value` from 0 to 1, such as a volume or a playback position.
function sample.bar(x, y, width, height, value, color)
    graphics2d.drawRect({x, y, width, height}, sample.idle, {layer = 1})
    graphics2d.drawRect({x, y, width * math.max(0, math.min(1, value)), height}, color or sample.accent, {layer = 2})
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
