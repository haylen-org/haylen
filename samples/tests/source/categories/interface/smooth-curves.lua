-- Every curved component and shape at the scale picked here, next to the pixels of their edges magnified: the GUI and the shapes draw into a capture of the whole screen, which shows on the screen as it is and in magnified crops, so the crops show the real pixels of every edge.
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local SmoothCurves = haylen.class('SmoothCurves', Test)

SmoothCurves.scales = {{id = 'design', text = 'Design 1x'}, {id = 'double', text = 'Design 2x'}, {id = 'physical', text = 'Physical'}}
SmoothCurves.settings = {design = {mode = 'design', factor = 1}, double = {mode = 'design', factor = 2}, physical = {mode = 'physical', factor = 1}}

-- The side of a crop in pixels of the screen.
SmoothCurves.cropPixels = 18

-- The nodes the crops magnify and the part of each that holds a curve: the top-left corner of rounded rectangles, the left side of the circles and pills that start their node in the middle of its height, or the upper left of a round node.
SmoothCurves.zooms = {
    {id = 'button', text = 'Button', part = 'corner'},
    {id = 'primary', text = 'Primary', part = 'corner'},
    {id = 'chip', text = 'Chip', part = 'corner'},
    {id = 'badge', text = 'Badge', part = 'corner'},
    {id = 'toggle', text = 'Switch', part = 'side'},
    {id = 'checkbox', text = 'Check box', part = 'side'},
    {id = 'radio', text = 'Radio', part = 'side'},
    {id = 'slider', text = 'Slider knob', part = 'side'},
    {id = 'progress', text = 'Progress', part = 'corner'},
    {id = 'ring', text = 'Circular progress', part = 'round'},
    {id = 'avatar', text = 'Avatar', part = 'round'},
    {id = 'field', text = 'Text field', part = 'corner'},
    {id = 'card', text = 'Card', part = 'corner'},
    {id = 'alert', text = 'Alert', part = 'corner'},
    {id = 'circle', text = 'Circle', part = 'shape'},
    {id = 'arc', text = 'Arc', part = 'shape'},
    {id = 'polygon', text = 'Polygon', part = 'shape'},
    {id = 'shape', text = 'Shape', part = 'shape'},
    {id = 'capsule', text = 'Capsule', part = 'shape'},
}

function SmoothCurves:enter()
    self.started = {mode = ui.scaleMode(), factor = ui.scale()}
    self.points = {}
    local crops = {}
    for index, zoom in ipairs(SmoothCurves.zooms) do
        crops[index] = ui.column{gap = 4, align = 'center',
            ui.spacer{id = 'zoom-' .. zoom.id, width = 112, height = 112},
            ui.label{text = zoom.text, font = 'caption', color = 'textMuted'},
        }
    end
    self:frame{
        hint = 'Pick a scale and compare the edges: every curve fades over about one pixel of the screen, the same at every scale, with no steps and no seams where shapes meet. The crops on the right magnify the top-left corner or the left side of each control.',
        focus = 'scale',
        content = {
            ui.row{gap = 24,
                ui.segmentedControl{id = 'scale', items = SmoothCurves.scales, selected = 'design', onChange = function(event) self:applyScale(event.value) end},
                ui.label{id = 'density', grow = 1, color = 'textMuted'},
            },
            ui.row{grow = 1, gap = 16,
                ui.scroll{id = 'cards', grow = 2, align = 'stretch', ui.grid{minColumnWidth = 360, gap = 16, children = self:cards()}},
                ui.panel{grow = 1, align = 'stretch', ui.scroll{id = 'crops', grow = 1, height = 0, ui.grid{minColumnWidth = 112, gap = 12, children = crops}}},
            },
        },
    }
end

function SmoothCurves:cards()
    return {
        layout.section('Buttons and badges', {
            ui.row{gap = 12, ui.button{id = 'button', text = 'Default'}, ui.button{id = 'primary', text = 'Primary', variant = 'primary'}},
            ui.row{gap = 12, ui.chip{id = 'chip', text = 'Chip', selected = true, removable = true}, ui.badge{id = 'badge', text = '12', tone = 'danger', solid = true}, ui.badge{text = 'New', tone = 'accent'}},
        }),
        layout.section('Choices', {
            ui.toggle{id = 'toggle', text = 'Switch', checked = true},
            ui.toggle{text = 'Off'},
            ui.checkbox{id = 'checkbox', text = 'Check box', checked = true},
            ui.radioGroup{id = 'radio', items = {{id = 'one', text = 'Radio'}, {id = 'two', text = 'Other'}}, selected = 'one', horizontal = true},
        }),
        layout.section('Values', {
            ui.slider{id = 'slider', value = 0},
            ui.progress{id = 'progress', value = 0.6, tone = 'success'},
            ui.row{gap = 16, ui.circularProgress{id = 'ring', value = 0.7}, ui.busyIndicator{}, ui.statusIndicator{text = 'Online'}},
        }),
        layout.section('Fields and panels', {
            ui.textField{id = 'field', placeholder = 'Name'},
            ui.row{gap = 12, ui.avatar{id = 'avatar', name = 'Ana Souza'}, ui.card{id = 'card', grow = 1, ui.label{text = 'Card'}}},
            ui.alert{id = 'alert', title = 'Alert', message = 'A rounded alert with its tone bar.', tone = 'warning'},
        }),
        layout.section('Shapes of "graphics2d"', {
            ui.spacer{id = 'shapes', height = 150},
        }),
    }
end

function SmoothCurves:applyScale(id)
    local setting = SmoothCurves.settings[id]
    ui.setScaleMode(setting.mode)
    ui.setScale(setting.factor)
end

function SmoothCurves:exit()
    ui.setScaleMode(self.started.mode)
    ui.setScale(self.started.factor)
    SmoothCurves.super.exit(self)
end

function SmoothCurves:update(dt)
    SmoothCurves.super.update(self, dt)
    local factor, scale, points = ui.scale()
    local pixels = viewport.pixelRect().width / viewport.visibleRect().width * scale
    self:status(string.format('Mode "%s", factor %.2f, %.2f design units, %.2f points and %.2f pixels per UI unit.', ui.scaleMode(), factor, scale, points, pixels))
    self:set('density', {text = string.format('One UI unit covers %.2f pixels of this screen.', pixels)})
end

-- The capture holds what the screen shows, pixel for pixel, so it has the size of the viewport in pixels.
function SmoothCurves:capture()
    local pixels = viewport.pixelRect()
    local width, height = math.max(1, math.floor(pixels.width + 0.5)), math.max(1, math.floor(pixels.height + 0.5))
    if not self.target or self.target.width ~= width or self.target.height ~= height then
        self.target = graphics.newRenderTarget(width, height)
    end
    return self.target
end

-- Circles of growing radius, a ring, an arc and a star polygon, and under them a card with a border over its soft shadow, a turned capsule and a pill rounded on one side, in UI units, so they grow with the scale like the components, with the points the crops magnify.
function SmoothCurves:drawShapes(area)
    local _, unit = ui.scale()
    local x, middle, lower = area.x, area.y + 40 * unit, area.y + 116 * unit
    local function at(offset)
        return x + offset * unit
    end
    graphics2d.drawCircle(at(8), middle, 5 * unit, Test.accent)
    graphics2d.drawCircle(at(30), middle, 12 * unit, Test.warm)
    graphics2d.drawCircle(at(76), middle, 26 * unit, Test.green)
    self.points.circle = {at(76 - 26 * 0.7071), middle - 26 * 0.7071 * unit}
    graphics2d.drawRing(at(132), middle, 24 * unit, 4 * unit, Test.violet)
    graphics2d.drawArc(at(190), middle, 24 * unit, 8 * unit, math.pi * 0.5, math.pi * 1.9, Test.red)
    self.points.arc = {at(190 - 28 * 0.7071), middle - 28 * 0.7071 * unit}
    local star = {}
    for index = 0, 9 do
        local radius = (index % 2 == 0 and 28 or 12) * unit
        local angle = index / 10 * m.tau - math.pi / 2
        star[#star + 1] = {at(256) + math.cos(angle) * radius, middle + math.sin(angle) * radius}
    end
    graphics2d.drawPolygon(star, Test.warm)
    self.points.polygon = {(star[1][1] + star[2][1]) * 0.5, (star[1][2] + star[2][2]) * 0.5}

    local radius = 12 * unit
    local card = {at(4), lower - 26 * unit, 110 * unit, 52 * unit}
    graphics2d.drawShape({card[1], card[2] + 6 * unit, card[3], card[4]}, {radius = radius, color = '#90000000', softness = 14 * unit})
    graphics2d.drawShape(card, {radius = radius, color = Test.surface, borderWidth = 2 * unit, borderColor = Test.accent})
    self.points.shape = {card[1] + radius * 0.2929, card[2] + radius * 0.2929}
    local turn = 0.35
    graphics2d.drawShape({at(136), lower - radius, 110 * unit, radius * 2}, {radius = radius, rotation = turn, color = Test.warm})
    self.points.capsule = {at(191) - math.cos(turn) * 55 * unit, lower - math.sin(turn) * 55 * unit}
    graphics2d.drawShape({at(250), lower - 14 * unit, 50 * unit, 28 * unit}, {radius = {14 * unit, 0, 0, 14 * unit}, color = Test.green})
end

-- The point a crop magnifies around: the top-left corner of a node, the middle of its left side, the upper left of a round node, or the point the shapes noted.
function SmoothCurves:cropPoint(zoom)
    if zoom.part == 'shape' then
        local point = self.points[zoom.id]
        return point and point[1], point and point[2]
    end
    local bounds = self.gui:bounds(zoom.id)
    if not bounds then
        return nil
    end
    if zoom.part == 'corner' then
        return bounds.x, bounds.y
    end
    if zoom.part == 'round' then
        local reach = math.min(bounds.width, bounds.height) * 0.5 * 0.7071
        return bounds.x + bounds.width * 0.5 - reach, bounds.y + bounds.height * 0.5 - reach
    end
    return bounds.x, bounds.y + bounds.height * 0.5
end

-- A crop reaches from a corner into its node, or centers on its point, and shows only while its point is in view, since a scroll can hide it.
function SmoothCurves:drawCrops(target, visible, pixels)
    local scaleX, scaleY = pixels.width / visible.width, pixels.height / visible.height
    local cards = self.gui:bounds('cards')
    local side = SmoothCurves.cropPixels
    graphics2d.pushClip(self.gui:bounds('crops'))
    for _, zoom in ipairs(SmoothCurves.zooms) do
        local slot = self.gui:bounds('zoom-' .. zoom.id)
        local x, y = self:cropPoint(zoom)
        if slot and x and cards and x >= cards.x and x <= cards.x + cards.width and y >= cards.y and y <= cards.y + cards.height then
            local cx, cy = (x - visible.x) * scaleX, (y - visible.y) * scaleY
            local centered = zoom.part == 'shape' or zoom.part == 'round'
            local left = centered and math.floor(cx - side * 0.5) or math.floor(cx) - 3
            local top = zoom.part == 'corner' and math.floor(cy) - 3 or math.floor(cy - side * 0.5)
            graphics2d.draw(target.texture, slot.x, slot.y, {source = {left, top, side, side}, width = slot.width, height = slot.height, pivotX = 0, pivotY = 0, blend = 'premultiplied'})
        end
    end
    graphics2d.popClip()
end

-- The shapes draw over the GUIs, inside the scroll of the cards.
function SmoothCurves:drawScrolledShapes(area)
    graphics2d.beginScreen({order = 1})
    graphics2d.pushClip(self.gui:bounds('cards'))
    self:drawShapes(area)
    graphics2d.popClip()
end

-- The capture begins after the screen canvas that shows it, so the GUIs drawn after this hook land in it and the canvas shows them in the same frame. A transition renders the scene into its own image, where the shapes draw straight.
function SmoothCurves:renderUi()
    local shapes = self.gui and self.gui:bounds('shapes')
    if not shapes then
        return
    end
    if graphics2d.capturing() then
        self:drawScrolledShapes(shapes)
        return
    end

    local target = self:capture()
    local visible = viewport.visibleRect()
    graphics2d.beginScreen()
    graphics2d.draw(target.texture, visible.x, visible.y, {width = visible.width, height = visible.height, pivotX = 0, pivotY = 0, blend = 'premultiplied'})
    self:drawCrops(target, visible, viewport.pixelRect())
    graphics2d.beginCapture(target)
    self:drawScrolledShapes(shapes)
end

return SmoothCurves
