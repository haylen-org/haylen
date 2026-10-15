-- Strokes and kept meshes: a line drawn with the pointer and stroked with the chosen join, cap, width and dashes, a tapered ribbon whose widths and colors blend along it, a rope of a tiled texture, and a grid mesh of the tree whose vertices wave in place with one call per frame.
local assets = require('haylen.assets')
local collections = require('haylen.collections')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local SpriteTest = require('categories.sprites.sprite-test')

local Strokes = haylen.class('Strokes', SpriteTest)

Strokes.joins = {{id = 'miter', text = 'Miter'}, {id = 'round', text = 'Round'}, {id = 'bevel', text = 'Bevel'}}
Strokes.caps = {{id = 'butt', text = 'Butt'}, {id = 'round', text = 'Round'}, {id = 'square', text = 'Square'}}
Strokes.maxPoints = 400

Strokes.code = [[
graphics2d.drawStroke(points, {count = count, width = 18, join = 'round', cap = 'round', dashes = {30, 14}})
graphics2d.drawStroke(rope, {width = 14, texture = ropeTexture, textureMode = 'tile'})
local flag = graphics2d.newGridMesh(tree, 12, 12)
flag:setPositions(moved)  -- One call moves every vertex.]]

function Strokes:enter()
    self.join, self.cap, self.width, self.dashed = 'round', 'round', 18, false
    self.points = collections.newFloatBuffer(Strokes.maxPoints * 2)
    self.count = 0
    self.time = 0
    -- The rope repeats its texture along its length, which takes a texture that wraps.
    self.rope = assets.texture('sprites/images/rope.png', {filter = 'linear', wrap = 'repeat'})
    local tree = SpriteTest.texture('images/tree.png')
    self.treeHeight = tree.height
    self.flag = graphics2d.newGridMesh(tree, 12, 12, {x = 0, y = 0, width = tree.width, height = tree.height})
    self.rest = collections.newFloatBuffer(self.flag.vertexCount * 2)
    self.flag:readPositions(self.rest)
    self.moved = collections.newFloatBuffer(#self.rest)
    self:frame{
        code = Strokes.code,
        hint = 'Draw on the play area with the mouse or a finger to stroke a line, and pick how it joins, caps and dashes.',
        controls = {
            ui.formField{label = 'Join', ui.segmentedControl{id = 'join', items = Strokes.joins, selected = self.join, onChange = function(event) self.join = event.value end}},
            ui.formField{label = 'Cap', ui.segmentedControl{id = 'cap', items = Strokes.caps, selected = self.cap, onChange = function(event) self.cap = event.value end}},
            ui.formField{label = 'Width', ui.slider{id = 'width', min = 1, max = 48, value = self.width, step = 1, showValue = true, onChange = function(event) self.width = event.value end}},
            ui.toggle{id = 'dashed', text = 'Dashes', onChange = function(event) self.dashed = event.checked end},
            ui.button{id = 'clear', text = 'Clear the line', onClick = function() self.count = 0 end},
        },
        focus = 'join',
    }
end

-- Adds the pointer to the line while it is held, a point every few units, and starts a new line when it goes down.
function Strokes:record()
    local x, y, pressed, held = self:pointer()
    if pressed then
        self.count = 0
    end
    if not held or self.count >= Strokes.maxPoints then
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

function Strokes:resize(area)
    -- A zigzag fills the line until the first drawing, so every join and cap shows at once.
    if self.count == 0 then
        for index = 0, 5 do
            self.count = self.count + 1
            self.points[self.count * 2 - 1] = area.width * (0.1 + index * 0.08)
            self.points[self.count * 2] = area.height * (index % 2 == 0 and 0.2 or 0.45)
        end
    end
end

function Strokes:update(dt)
    Strokes.super.update(self, dt)
    self.time = self.time + dt
    self:record()
    local rest, moved, time = self.rest, self.moved, self.time
    for index = 1, #rest, 2 do
        local x, y = rest[index], rest[index + 1]
        moved[index] = x + math.sin(time * 3 + y * 0.02) * (1 - y / self.treeHeight) * 36
        moved[index + 1] = y
    end
    self.flag:setPositions(moved)
    local stats = graphics2d.stats()
    self:status(string.format('Line points %d   Mesh vertices %d   Draw calls %d   Vertices %d', self.count, self.flag.vertexCount, stats.drawCalls, stats.vertices))
end

function Strokes:draw(area)
    if self.count >= 2 then
        graphics2d.drawStroke(self.points, {count = self.count, width = self.width, join = self.join, cap = self.cap, color = '#FF7FD4FF', dashes = self.dashed and {self.width * 1.5, self.width} or nil, layer = 2})
    end
    local width, height = area.width, area.height
    local ribbon = {width * 0.1, height * 0.75, width * 0.25, height * 0.62, width * 0.4, height * 0.8, width * 0.55, height * 0.66}
    graphics2d.drawStroke(ribbon, {widths = {2, 28, 18, 2}, colors = {'#FFFFD54F', '#FFFF7043', '#FFAB47BC', '#FF29B6F6'}, join = 'round', cap = 'round', layer = 1})
    local rope = {width * 0.1, height * 0.92, width * 0.3, height * 0.85 + math.sin(self.time) * 20, width * 0.55, height * 0.92}
    graphics2d.drawStroke(rope, {width = 14, texture = self.rope, textureMode = 'tile', join = 'round', layer = 1})
    self.flag:draw(width * 0.66, height * 0.1, {layer = 1, scaleX = 1.5, scaleY = 1.5})
end

return Strokes
