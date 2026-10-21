-- The base of the text tests, which draw on a page: the page fills the play area at the zoom that fits its smallest size and a margin around it, and the room the area has beyond that size widens or lengthens `self.layout`, whose top-left corner is 0, 0, so a test spreads its content over the whole area at the same scale on every screen.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local m = require('haylen.math')
local viewport = require('haylen.viewport')

local Test = require('harness.test')

local TextTest = haylen.class('TextTest', Test)

TextTest.wide = {1800, 620}
TextTest.narrow = {1340, 620}
TextTest.margin = 40
TextTest.stretch = 1.4

TextTest.labelColor = '#FFB4BCD4'
TextTest.boxColor = '#FF202738'
TextTest.boxBorder = '#FF39415C'
TextTest.boxRadius = 12

-- Takes the options of `Test:frame` and `page`, the smallest size of the page, which defaults to the narrow page next to a panel of controls and to the wide one without it.
function TextTest:frame(options)
    self.page = options.page or (options.controls and TextTest.narrow or TextTest.wide)
    self.layout = m.rect(0, 0, self.page[1], self.page[2])
    TextTest.super.frame(self, options)
end

-- The page grows with the area up to a share of its smallest size on each side and stays centered in the room past that, so its content never drifts far apart.
function TextTest:resize(area)
    local margin, page, stretch = TextTest.margin, self.page, TextTest.stretch
    local zoom = math.min(area.width / (page[1] + margin * 2), area.height / (page[2] + margin * 2))
    local roomWidth, roomHeight = area.width / zoom, area.height / zoom
    local width = math.min(roomWidth - margin * 2, page[1] * stretch)
    local height = math.min(roomHeight - margin * 2, page[2] * stretch)
    self.camera.zoom = {zoom, zoom}
    self.camera.position = {-(roomWidth - width) / 2, -(roomHeight - height) / 2}
    self.layout = m.rect(0, 0, width, height)
    self.zoom = zoom
end

-- Returns the size nearest to `multiple` times the native size of a bitmap font at which each pixel of the font covers a whole number of pixels of the screen, so its strokes keep one width.
function TextTest:pixelSize(font, multiple)
    local pixels = self:screenPixels()
    return font.nativeSize * math.max(1, math.floor(multiple * pixels + 0.5)) / pixels
end

-- Returns the size at which each pixel of a bitmap font covers `count` pixels of the screen.
function TextTest:screenPixelSize(font, count)
    return font.nativeSize * count / self:screenPixels()
end

-- Returns how many pixels of the screen one unit of the page covers.
function TextTest:screenPixels()
    return viewport.pixelsPerUnit() * (self.zoom or 1)
end

-- Returns the point of the page nearest to `x`, `y` that lands on a whole pixel of the screen, where rich text in a bitmap font starts so its pixels stay whole.
function TextTest:onPixel(x, y)
    local pixelX, pixelY = viewport.toFramebuffer(self.camera:worldToScreen(x, y))
    return self.camera:screenToWorld(viewport.toDesign(math.floor(pixelX + 0.5), math.floor(pixelY + 0.5)))
end

-- Draws a label of the page, larger and brighter than the caption of the harness.
function TextTest.label(text, x, y, options)
    options = options or {}
    graphics2d.drawText(nil, text, x, y, {size = options.size or 26, color = options.color or TextTest.labelColor, anchor = options.anchor or {0, 0}, maxWidth = options.maxWidth or 0, align = options.align or 'start', layer = options.layer or 2})
end

-- Draws the box that holds an example, with rounded corners and a thin border.
function TextTest.box(rect, options)
    options = options or {}
    graphics2d.drawShape(rect, {radius = options.radius or TextTest.boxRadius, color = options.color or TextTest.boxColor, borderWidth = options.borderWidth or 2, borderColor = options.borderColor or TextTest.boxBorder, layer = options.layer or -10})
end

-- Paints the whole play area, past the page and its margins.
function TextTest.fill(color)
    graphics2d.drawRect(graphics2d.canvasBounds(), color, {layer = -900})
end

return TextTest
