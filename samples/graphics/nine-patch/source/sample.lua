-- The frame every test shares: the Back button with the title and the description, the stage the test draws in, the code the test runs under the stage, a panel of controls on the right, a status line and a line of hints.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = {}

local kTransition = {effect = 'fade', duration = 0.3, color = '#FF101418'}
local kPanelWidth = 440
local kStageColor = '#FF181D26'
local kStatusInterval = 0.2

sample.ink = '#FFE8EAF2'
sample.muted = '#FF7A8099'
sample.line = '#FF2C3147'
sample.accent = '#FF4C7DFF'
sample.warm = '#FFF2B23A'
sample.green = '#FF3DBE7A'
sample.red = '#FFE5534B'

local Test = haylen.class('Test', scene.Scene)
sample.Test = Test

-- The stage camera shows stage-local coordinates: 0, 0 is the top-left corner of the stage wherever the layout puts it.
function Test:init(info)
    self.info = info
    self.statusTime = kStatusInterval
    self.camera = graphics2d.newCamera()
    self.camera.anchor = 'topLeft'
end

-- Mounts the frame. `options` holds the `hint`, the `code` shown under the stage, the `controls` of the panel and the id of the control to `focus` for gamepads and TV remotes.
function Test:frame(options)
    window.setBackLeavesApp(false)
    local left = {ui.spacer{id = 'stage', grow = 1}}
    if options.code then
        left[2] = ui.card{ui.label{id = 'code', text = options.code, font = 'monospace'}}
    end
    local middle = {ui.column{grow = 1, gap = 12, align = 'stretch', children = left}}
    if options.controls then
        middle[2] = ui.panel{id = 'panel', width = kPanelWidth, align = 'stretch', ui.scroll{grow = 1, ui.column{gap = 12, children = options.controls}}}
    end

    self.document = ui.mount(ui.column{
        padding = 24,
        gap = 12,
        ui.row{gap = 24, align = 'start',
            ui.button{id = 'back', text = 'Back', onClick = sample.back},
            ui.column{grow = 1, gap = 4,
                ui.label{text = self.info.title, font = 'heading'},
                ui.label{text = self.info.description, color = 'textMuted'},
            },
        },
        ui.row{grow = 1, gap = 16, children = middle},
        ui.label{id = 'status', text = '', font = 'monospace', color = 'accentText'},
        ui.label{text = (options.hint and options.hint .. ' ' or '') .. 'Escape, the east button or Back returns to the menu.', font = 'caption', color = 'textMuted'},
    }, {owner = self})
    self.document:command(options.focus or 'back', 'focus')
end

-- Goes back on the back action and follows the stage, calling `resize` with the stage-local area whenever its size changes. Tests call it first from their own update.
function Test:update(dt)
    if input.pressed('back') then
        sample.back()
    end
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

-- Draws the stage background and lets the test draw in stage-local coordinates once the stage has a size. `canvas` holds the options of the world canvas, such as its sort.
function Test:render()
    if self.area then
        graphics2d.beginWorld(self.camera, self.canvas)
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

-- Returns the pointer in stage-local coordinates, whether it went down this frame and whether it is held, from the first finger or the mouse, ignoring the interface.
function Test:pointer()
    local touch = input.touches()[1]
    if touch then
        local x, y = self:toStage(touch.x, touch.y)
        local held = touch.phase ~= 'ended' and touch.phase ~= 'cancelled' and not ui.wantsPointer()
        return x, y, touch.phase == 'began' and held, held
    end
    local x, y = self:toStage(input.mousePosition())
    local free = not ui.wantsPointer()
    return x, y, input.mousePressed('left') and free, input.mouseDown('left') and free
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

-- Loads an image of the sample with smooth filtering, or with `filter` when given.
function sample.texture(path, filter)
    return assets.texture(path, {filter = filter or 'linear'})
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
