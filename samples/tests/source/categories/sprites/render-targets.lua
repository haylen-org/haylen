-- Render targets: a little scene of a walker running across a meadow drawn offscreen every frame, then used as a texture as it is, turned and scaled, tinted and flipped, and on a waving mesh.
local animation2d = require('haylen.animation2d')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local Landscape = require('categories.sprites.landscape')
local Plate = require('categories.sprites.plate')
local SpriteTest = require('categories.sprites.sprite-test')

local RenderTargets = haylen.class('RenderTargets', SpriteTest)

-- The target is taller than wide, so the four pictures fill the height of a wide stage.
RenderTargets.width = 240
RenderTargets.height = 320
RenderTargets.columns = 16
RenderTargets.labels = {'As it is', 'Turned and scaled', 'Tinted and flipped', 'On a waving mesh'}
-- The meadow in the target moves this many pixels per second under the walker.
RenderTargets.travel = 90

RenderTargets.code = [[
local target = graphics.newRenderTarget(240, 320, {filter = 'linear'})
graphics2d.beginTarget(target, targetCamera, {clear = '#FF203040'})  -- Any drawing, every frame or once.
graphics2d.beginWorld(camera)
graphics2d.draw(target.texture, x, y, {width = 210, height = 280, rotation = 0.3})
graphics2d.draw(target.texture, x, y, {color = '#FFFF9060', flipVertical = true})
graphics2d.drawMesh(target.texture, vertices, indices)  -- The same texture on a waving strip.]]

function RenderTargets:enter()
    local columns = RenderTargets.columns
    self.target = graphics.newRenderTarget(RenderTargets.width, RenderTargets.height, {filter = 'linear'})
    self.targetCamera = graphics2d.newCamera()
    self.targetCamera.position = {RenderTargets.width / 2, RenderTargets.height / 2}
    self.landscape = Landscape()
    local sheet = SpriteTest.texture('sheets/walker.png')
    self.walker = graphics2d.newSprite(sheet, {scaleX = 0.46, scaleY = 0.46, layer = 2})
    self.animator = animation2d.newAnimator()
    self.animator:add('run', animation2d.fromGrid(sheet, {frameWidth = 256, frameHeight = 256, cells = {9, 10, 11, 12, 13, 14, 15, 16}, framesPerSecond = 12}))
    self.animator.pivotY = 0.97
    self.animator:play('run')
    self.tree = SpriteTest.texture('images/tree.png')
    self.time = 0
    self.redraw = true
    self.vertices, self.indices = {}, {}
    for column = 0, columns do
        self.vertices[column * 2 + 1] = {u = column / columns, v = 0}
        self.vertices[column * 2 + 2] = {u = column / columns, v = 1}
        if column < columns then
            local a = column * 2 + 1
            table.move({a, a + 1, a + 2, a + 1, a + 3, a + 2}, 1, 6, #self.indices + 1, self.indices)
        end
    end
    self:frame{
        code = RenderTargets.code,
        hint = 'Turn the redraw off and the target keeps the last frame it drew.',
        controls = {ui.toggle{id = 'redraw', text = 'Redraw every frame', checked = true, onChange = function(event) self.redraw = event.checked end}},
        focus = 'redraw',
    }
end

function RenderTargets:update(dt)
    RenderTargets.super.update(self, dt)
    self.time = self.time + dt
    if self.redraw then
        self.animator:update(dt)
        self.animator:apply(self.walker)
    end
    self:status(self.redraw and string.format('The target of %d by %d pixels is drawn again every frame and used four times.', RenderTargets.width, RenderTargets.height) or 'The target keeps the last frame it drew, and the four pictures stand still.')
end

-- Draws the little scene into the target: the meadow moves left under the walker, and a tree passes now and then.
function RenderTargets:drawTarget()
    local width, height, travel = RenderTargets.width, RenderTargets.height, self.time * RenderTargets.travel
    local area = {width = width, height = height}
    local ground = height - 58
    graphics2d.beginTarget(self.target, self.targetCamera, {clear = '#FF4FA8EC'})
    self.landscape:draw(area, ground, 0.24, self.time, travel)
    local treeX = width + 80 - travel % (width + 160)
    graphics2d.draw(self.tree, treeX, ground + 4, {pivotY = 1, scaleX = 0.7, scaleY = 0.7, layer = 1})
    self.walker.x, self.walker.y = width * 0.42, ground
    graphics2d.drawEllipse(self.walker.x, ground - 2, 30, 7, '#40000000', {layer = 1})
    self.walker:draw()
    graphics2d.drawText(nil, string.format('%.1f s', self.time), 14, 10, {size = 30, color = '#FFFFFFFF', outlineWidth = 3, outlineColor = '#FF1E1B33', layer = 3})
end

-- The target draws before the stage, which then shows its texture.
function RenderTargets:render()
    if self.redraw then
        self:drawTarget()
    end
    RenderTargets.super.render(self)
end

-- Lays the four pictures in a row on a wide stage and in a grid of two by two on a tall one, each centered in its cell above its label, and returns their centers and size.
function RenderTargets:cells(area)
    local columns = area.height > area.width * 0.75 and 2 or 4
    local rows = 4 // columns
    local cellWidth, cellHeight = area.width / columns, area.height / rows
    local width = math.min(cellWidth - 72, (cellHeight - 120) * RenderTargets.width / RenderTargets.height)
    local height = width * RenderTargets.height / RenderTargets.width
    local cells = {}
    for index = 1, 4 do
        local column, row = (index - 1) % columns, (index - 1) // columns
        cells[index] = {x = cellWidth * (column + 0.5), y = cellHeight * row + (cellHeight - 60) / 2}
    end
    return cells, width, height
end

function RenderTargets:draw(area)
    local texture = self.target.texture
    local cells, width, height = self:cells(area)
    local time = self.time
    for index, cell in ipairs(cells) do
        local halfWidth, halfHeight = width / 2 + 14, height / 2 + 14
        graphics2d.drawShape({cell.x - halfWidth, cell.y - halfHeight, halfWidth * 2, halfHeight * 2}, {radius = 18, color = '#FF1F2436', borderWidth = 2, borderColor = '#FF3A4160', layer = -2})
        Plate.draw(RenderTargets.labels[index], cell.x, cell.y + halfHeight + 32)
    end

    local scale = 0.82 + math.sin(time * 1.3) * 0.12
    graphics2d.draw(texture, cells[1].x, cells[1].y, {width = width, height = height})
    graphics2d.draw(texture, cells[2].x, cells[2].y, {width = width * scale, height = height * scale, rotation = math.sin(time) * 0.45})
    graphics2d.draw(texture, cells[3].x, cells[3].y, {width = width, height = height, color = '#FFFF9060', flipVertical = true})

    local columns = RenderTargets.columns
    local left, top = cells[4].x - width / 2, cells[4].y - height / 2
    local amplitude = height * 0.05
    for column = 0, columns do
        local lift = math.sin(time * 3 + column * 0.6) * amplitude
        local x = left + width * column / columns
        self.vertices[column * 2 + 1].x, self.vertices[column * 2 + 1].y = x, top + lift
        self.vertices[column * 2 + 2].x, self.vertices[column * 2 + 2].y = x, top + height + lift
    end
    graphics2d.drawMesh(texture, self.vertices, self.indices)
end

return RenderTargets
