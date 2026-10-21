-- Strokes and kept meshes, each in a card of its own: a line drawn with the pointer on a pad and stroked with the chosen join, cap, width and dashes, a streamer whose widths and colors blend along it, a rope of a tiled texture that sags between two rings, and a grid mesh of a tree whose vertices sway in place with one call per frame.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Test = require('harness.test')

local Strokes = haylen.class('Strokes', SpriteTest)

Strokes.joins = {{id = 'miter', text = 'Miter'}, {id = 'round', text = 'Round'}, {id = 'bevel', text = 'Bevel'}}
Strokes.caps = {{id = 'butt', text = 'Butt'}, {id = 'round', text = 'Round'}, {id = 'square', text = 'Square'}}
Strokes.maxPoints = 400
Strokes.streamerPoints = 10
Strokes.streamerColors = {'#FFFFD54F', '#FFFFB74D', '#FFFF8A65', '#FFF06292', '#FFBA68C8', '#FF9575CD', '#FF7986CB', '#FF64B5F6', '#FF4FC3F7', '#FF4DD0E1'}
Strokes.grass = {2, 2, 128, 128}

-- The cards of each layout in world units around the center of the view, which the camera fits into the play area.
Strokes.layouts = {
    landscape = {view = {1700, 800}, cards = {pad = {-830, -380, 1080, 460}, streamer = {-830, 100, 530, 280}, rope = {-280, 100, 530, 280}, tree = {270, -380, 560, 760}}},
    portrait = {view = {900, 1640}, cards = {pad = {-430, -800, 860, 560}, streamer = {-430, -220, 860, 260}, rope = {-430, 60, 860, 260}, tree = {-430, 340, 860, 460}}},
}

Strokes.code = [[
graphics2d.drawStroke(points, {count = count, width = 18, join = 'round', cap = 'round', dashes = {27, 18}})
graphics2d.drawStroke(streamer, {widths = widths, colors = colors, join = 'round', cap = 'round'})
graphics2d.drawStroke(rope, {width = 22, texture = ropeTexture, textureMode = 'tile', join = 'round'})
local tree = graphics2d.newGridMesh(treeTexture, 12, 12)
tree:setPositions(moved)  -- One call moves every vertex.]]

function Strokes:enter()
    self.join, self.cap, self.width, self.dashed = 'round', 'round', 18, false
    self.points = collections.newFloatBuffer(Strokes.maxPoints * 2)
    self.count = 0
    self.time = 0
    self.streamer = {}
    self.widths = {}
    for index = 1, Strokes.streamerPoints do
        local share = (index - 1) / (Strokes.streamerPoints - 1)
        self.widths[index] = 3 + math.sin(share * math.pi) * 34
    end
    -- The rope repeats its texture along its length, which takes a texture that wraps.
    self.rope = assets.texture('sprites/images/rope.png', {filter = 'linear', wrap = 'repeat', mipmaps = true})
    self.terrain = assets.texture('tiled/tiles/terrain.png', {filter = 'linear', mipmaps = true})
    local tree = SpriteTest.texture('images/tree.png')
    self.treeWidth, self.treeHeight = tree.width, tree.height
    self.tree = graphics2d.newGridMesh(tree, 12, 12, {x = 0, y = 0, width = tree.width, height = tree.height})
    self.rest = collections.newFloatBuffer(self.tree.vertexCount * 2)
    self.tree:readPositions(self.rest)
    self.moved = collections.newFloatBuffer(#self.rest)
    self.layout = Strokes.layouts.landscape
    self:frame{
        code = Strokes.code,
        view = self.layout.view,
        hint = 'Draw on the pad with the mouse or a finger to stroke a line, and pick how it joins, caps and dashes.',
        controls = {
            ui.formField{label = 'Join', ui.segmentedControl{id = 'join', items = Strokes.joins, selected = self.join, onChange = function(event) self.join = event.value end}},
            ui.formField{label = 'Cap', ui.segmentedControl{id = 'cap', items = Strokes.caps, selected = self.cap, onChange = function(event) self.cap = event.value end}},
            ui.formField{label = 'Width', ui.slider{id = 'width', min = 2, max = 48, value = self.width, step = 1, showValue = true, decimals = 0, onChange = function(event) self.width = event.value end}},
            ui.toggle{id = 'dashed', text = 'Dashes', onChange = function(event) self.dashed = event.checked end},
            ui.button{id = 'clear', text = 'Clear the pad', onClick = function() self.count = 0 end},
        },
        focus = 'join',
    }
end

-- Returns the area of the pad that takes the line, inside its card.
function Strokes:padArea()
    local rect = self.layout.cards.pad
    return rect[1] + 30, rect[2] + 80, rect[3] - 60, rect[4] - 110
end

-- Adds the pointer to the line while it is held on the pad, a point every few units, and starts a new line when it goes down there.
function Strokes:record()
    local x, y, pressed, held = self:pointer()
    local left, top, width, height = self:padArea()
    local inside = x >= left and x <= left + width and y >= top and y <= top + height
    if pressed and inside then
        self.count = 0
    end
    if not held or not inside or self.count >= Strokes.maxPoints then
        return
    end
    if self.count > 0 then
        local lastX, lastY = self.points[self.count * 2 - 1], self.points[self.count * 2]
        if (x - lastX) ^ 2 + (y - lastY) ^ 2 < 36 then
            return
        end
    end
    self.count = self.count + 1
    self.points[self.count * 2 - 1], self.points[self.count * 2] = x, y
end

-- A zigzag fills the pad until the first drawing, so every join and cap shows at once.
function Strokes:zigzag()
    local left, top, width, height = self:padArea()
    self.count = 0
    for index = 0, 6 do
        self.count = self.count + 1
        self.points[self.count * 2 - 1] = left + width * (0.06 + index * 0.147)
        self.points[self.count * 2] = top + height * (index % 2 == 0 and 0.2 or 0.8)
    end
end

-- Picks the layout that draws the cards larger in the play area, and fills its pad with the zigzag again.
function Strokes:resize(area)
    self.layout = Panels.layout(Strokes.layouts, area)
    self.view = self.layout.view
    self:zigzag()
end

function Strokes:update(dt)
    Strokes.super.update(self, dt)
    self.time = self.time + dt
    self:record()
    local rest, moved, time, height = self.rest, self.moved, self.time, self.treeHeight
    for index = 1, #rest, 2 do
        local x, y = rest[index], rest[index + 1]
        moved[index] = x + math.sin(time * 2.4 + y * 0.02) * (1 - y / height) ^ 2 * 22
        moved[index + 1] = y
    end
    self.tree:setPositions(moved)
    self:status(string.format('The line holds %d points, and the tree mesh moves %d vertices in one call.', self.count, self.tree.vertexCount))
end

function Strokes:drawPad(rect)
    Panels.card(rect, 'Draw a line on the pad')
    local left, top, width, height = self:padArea()
    graphics2d.drawShape({left, top, width, height}, {radius = 12, color = '#FF171C29', borderWidth = 2, borderColor = Test.line, layer = -5})
    if self.count >= 2 then
        graphics2d.drawStroke(self.points, {count = self.count, width = self.width, join = self.join, cap = self.cap, color = '#FF7FD4FF', dashes = self.dashed and {self.width * 1.5, self.width} or nil, layer = 2})
    end
end

-- A streamer that waves along its length, thin at both ends and wide in the middle, from yellow to blue.
function Strokes:drawStreamer(rect)
    local inner = Panels.card(rect, 'Widths and colors blend along it')
    local streamer, count = self.streamer, Strokes.streamerPoints
    for index = 1, count do
        local share = (index - 1) / (count - 1)
        streamer[index * 2 - 1] = inner[1] + 20 + share * (inner[3] - 40)
        streamer[index * 2] = inner[2] + inner[4] / 2 + math.sin(self.time * 2.2 - share * 5) * inner[4] * 0.28
    end
    graphics2d.drawStroke(streamer, {widths = self.widths, colors = Strokes.streamerColors, join = 'round', cap = 'round', layer = 1})
end

-- A rope that sags between two brass rings and sways a little.
function Strokes:drawRope(rect)
    local inner = Panels.card(rect, 'A rope of a tiled texture')
    local leftX, rightX, y = inner[1] + 30, inner[1] + inner[3] - 30, inner[2] + 24
    local sag = inner[4] * 0.62 + math.sin(self.time * 1.3) * 10
    local rope = {}
    for index = 0, 16 do
        local share = index / 16
        rope[index * 2 + 1] = leftX + (rightX - leftX) * share + math.sin(self.time * 1.3) * 14 * math.sin(share * math.pi)
        rope[index * 2 + 2] = y + sag * 4 * share * (1 - share)
    end
    graphics2d.drawStroke(rope, {width = 22, texture = self.rope, textureMode = 'tile', join = 'round', layer = 1})
    for _, x in ipairs({leftX, rightX}) do
        graphics2d.drawShape({x - 16, y - 16, 32, 32}, {kind = 'ellipse', color = '#00000000', borderWidth = 8, borderColor = '#FFE0A93B', layer = 2})
        graphics2d.drawShape({x - 20, y - 20, 40, 40}, {kind = 'ellipse', color = '#00000000', borderWidth = 3, borderColor = '#FF1E1B33', layer = 3})
    end
end

-- The tree sways on a strip of grass, its trunk fixed and its crown moving the most.
function Strokes:drawTree(rect)
    local inner = Panels.card(rect, 'A kept mesh that sways')
    local ground = math.min(130, inner[4] * 0.28)
    local groundTop = inner[2] + inner[4] - ground
    graphics2d.pushMask(function()
        graphics2d.drawShape({inner[1], groundTop, inner[3], ground}, {radius = 14})
    end, {layer = -5})
    for x = inner[1], inner[1] + inner[3] - 1, ground do
        graphics2d.draw(self.terrain, x, groundTop, {source = Strokes.grass, width = ground, height = ground, pivotX = 0, pivotY = 0})
    end
    graphics2d.popMask()
    local scale = math.min((inner[4] - ground * 0.4) / self.treeHeight, inner[3] * 0.8 / self.treeWidth)
    local x, y = inner[1] + inner[3] / 2 - self.treeWidth * scale / 2, groundTop + ground * 0.55 - self.treeHeight * scale
    graphics2d.drawEllipse(inner[1] + inner[3] / 2, groundTop + ground * 0.5, self.treeWidth * scale * 0.42, ground * 0.14, '#50101820', {layer = -4})
    self.tree:draw(x, y, {scaleX = scale, scaleY = scale, layer = 1})
end

function Strokes:draw(area)
    local cards = self.layout.cards
    self:drawPad(cards.pad)
    self:drawStreamer(cards.streamer)
    self:drawRope(cards.rope)
    self:drawTree(cards.tree)
end

return Strokes
