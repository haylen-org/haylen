-- Screen to world: `camera:screenToWorld` turns the pointer into a world point through zoom and rotation, which picks the object under it, and `camera:worldToScreen` places a screen label over the pick.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local ui = require('haylen.ui')

local CameraTest = require('categories.camera.camera-test')
local Terrain = require('categories.sprites.terrain')
local Test = require('harness.test')
local World = require('categories.camera.world')
local actions = require('categories.camera.actions')

local Picking = haylen.class('Picking', CameraTest)

Picking.code = [[
local x, y = input.mousePosition()
local worldX, worldY = camera:screenToWorld(x, y)  -- Through the zoom and the rotation of the view.
local screenX, screenY = camera:worldToScreen(selected.x, selected.y)
graphics2d.beginScreen()
graphics2d.drawText(nil, selected.name, screenX, screenY - 60, {size = 30, anchor = {0.5, 1}})]]

Picking.cursorSpeed = 900
Picking.area = {-1100, -650, 2200, 1300}
Picking.cell = 100
Picking.pond = {x = 420, y = -180, radiusX = 330, radiusY = 220}
-- The things on the clearing, by their kind of prop and the name the label shows.
Picking.things = {
    {'chest', 'Treasure chest'}, {'crate', 'Crate'}, {'barrel', 'Barrel'}, {'well', 'Well'}, {'tree', 'Oak'}, {'pine', 'Pine'},
    {'rock', 'Mossy rock'}, {'bush', 'Bush'}, {'flowers', 'Flowering bush'}, {'mushrooms', 'Mushrooms'}, {'stump', 'Stump'}, {'log', 'Fallen log'},
    {'signpost', 'Signpost'}, {'lamp', 'Lamp post'}, {'campfire', 'Campfire'}, {'flag', 'Flag'},
}

function Picking:init(entry)
    Picking.super.init(self, entry)
    self.camera = graphics2d.newCamera()
    self.camera.zoom = {1.2, 1.2}
    self.camera.rotation = 0.3
    self.worldX, self.worldY = 0, 0

    local area, cell = Picking.area, Picking.cell
    self.ground, self.edges = Terrain.bake(area[3] // cell, area[4] // cell, cell, area[1], area[2], function(column, row)
        return Picking.kindAt(area[1] + column * cell, area[2] + row * cell)
    end)
    self:scatter()
end

-- Returns the kind of ground at a point of the clearing: a pond, a path that curves around it, and grass.
function Picking.kindAt(x, y)
    local pond = Picking.pond
    if ((x - pond.x) / pond.radiusX) ^ 2 + ((y - pond.y) / pond.radiusY) ^ 2 < 1 then
        return Terrain.water
    end
    if math.abs(y - 260 - math.sin(x / 300) * 120) < 70 then
        return Terrain.path
    end
    return Terrain.grass
end

-- Places two of every thing on the grass, apart from each other, and draws them from the top down.
function Picking:scatter()
    local area = Picking.area
    local points = m.poissonDisk({area = {area[1] + 120, area[2] + 160, area[3] - 240, area[4] - 220}, minimumDistance = 200, seed = 9, accept = function(point)
        return Picking.kindAt(point.x, point.y) == Terrain.grass and Picking.kindAt(point.x, point.y - 40) == Terrain.grass
    end})
    table.sort(points, function(a, b) return a.y < b.y end)
    local textures = {}
    for index, path in ipairs(World.textures) do
        textures[index] = World.texture(path)
    end
    self.textures = textures
    self.batch = graphics2d.newSpriteBatch(textures)
    self.objects = {}
    for index, point in ipairs(points) do
        local thing = Picking.things[(index - 1) % #Picking.things + 1]
        local spec = World.kinds[thing[1]]
        local object = {name = thing[2], x = point.x, y = point.y, spec = spec, flip = index % 2 == 0}
        object.box = m.rect(point.x - spec.width / 2, point.y - spec.height * spec.pivotY, spec.width, spec.height)
        self.batch:add({x = point.x, y = point.y, width = spec.width, height = spec.height, pivotY = spec.pivotY, texture = spec.texture, source = spec.source, flipHorizontal = object.flip})
        self.objects[#self.objects + 1] = object
    end
end

function Picking:enter()
    self:loadActions(actions)
    self:frame{
        hint = 'Point with the mouse, a finger, WASD, the arrows or the left stick, and select with a click, a tap, F or the X button. Q and E turn the view, Z and X zoom it.',
        code = Picking.code,
        play = true,
    }
end

-- Returns the object in front whose picture holds the world point: the lowest one, which the view draws over the others.
function Picking:objectAt(x, y)
    for index = #self.objects, 1, -1 do
        local object = self.objects[index]
        if object.box:contains({x, y}) then
            return object
        end
    end
end

-- Returns the screen point of the middle of the thing nearest the middle of the clearing, where the cursor starts so the test shows a pick at once.
function Picking:startPoint()
    local nearest
    for _, object in ipairs(self.objects) do
        if not nearest or object.x ^ 2 + object.y ^ 2 < nearest.x ^ 2 + nearest.y ^ 2 then
            nearest = object
        end
    end
    local box = nearest.box
    return self.camera:worldToScreen(box.x + box.width / 2, box.y + box.height / 2)
end

-- Moves the cursor, a screen point inside the play area, with the mouse, the first finger and the move action, and tells whether it selected this frame.
function Picking:moveCursor(dt, stage)
    if not self.cursorX then
        self.cursorX, self.cursorY = self:startPoint()
    end
    local selected = input.pressed('place')
    local touch = input.touches()[1]
    local mouseX, mouseY = input.mouseDelta()
    if touch and not ui.usingPointer() and stage:contains({touch.x, touch.y}) then
        self.cursorX, self.cursorY = touch.x, touch.y
        selected = selected or touch.phase == 'began'
    elseif mouseX ~= 0 or mouseY ~= 0 or input.mousePressed('left') then
        local x, y = input.mousePosition()
        if stage:contains({x, y}) and not ui.usingPointer() then
            self.cursorX, self.cursorY = x, y
            selected = selected or input.mousePressed('left')
        end
    end

    local moveX, moveY = input.vector('move')
    self.cursorX = m.clamp(self.cursorX + moveX * Picking.cursorSpeed * dt, stage.x, stage:right())
    self.cursorY = m.clamp(self.cursorY + moveY * Picking.cursorSpeed * dt, stage.y, stage:bottom())
    return selected
end

function Picking:update(dt)
    Picking.super.update(self, dt)
    if not self.stage then
        return
    end
    local selected = self:moveCursor(dt, self.stage)
    local camera = self.camera
    camera.rotation = camera.rotation + input.value('rotate') * dt
    local zoom = m.clamp(camera.zoom.x * (1 + input.value('zoom') * dt), 0.6, 2.5)
    camera.zoom = {zoom, zoom}

    self.worldX, self.worldY = camera:screenToWorld(self.cursorX, self.cursorY)
    self.hovered = self:objectAt(self.worldX, self.worldY)
    if selected then
        self.selected = self.hovered
    end
    local name = self.hovered and self.hovered.name:lower() or 'the grass'
    self:status(string.format('The pointer at %d, %d on the screen is at %d, %d in the world, over %s.', math.floor(self.cursorX + 0.5), math.floor(self.cursorY + 0.5), math.floor(self.worldX + 0.5), math.floor(self.worldY + 0.5), name))
end

-- Draws an object again over the others with an outline around its picture.
function Picking:highlight(object, color)
    local spec = object.spec
    graphics2d.draw(self.textures[spec.texture], object.x, object.y, {source = spec.source, width = spec.width, height = spec.height, pivotY = spec.pivotY, flipHorizontal = object.flip, effect = {outlineWidth = 10, outlineColor = color}, layer = 2})
end

function Picking:draw(area)
    graphics2d.drawStatic(self.ground, 0, 0, {layer = -3})
    graphics2d.drawStatic(self.edges, 0, 0, {layer = -2})
    self.batch:draw({layer = 1})
    if self.hovered and self.hovered ~= self.selected then
        self:highlight(self.hovered, '#FFFFFFFF')
    end
    if self.selected then
        self:highlight(self.selected, '#FFFFC93C')
    end
end

function Picking:renderUi()
    if not self.cursorX then
        return
    end
    graphics2d.beginScreen()
    graphics2d.pushClip(self.stage)
    -- The label names the selected thing, or the one under the cursor until something is selected.
    local shown = self.selected or self.hovered
    if shown then
        local box = shown.box
        local x, top = self.camera:worldToScreen(box.x + box.width / 2, box.y)
        local _, feet = self.camera:worldToScreen(shown.x, shown.y)
        -- A thing near the top of the play area takes its label under its feet.
        local y = top - 12 < self.stage.y + 60 and feet + 60 or top - 12
        CameraTest.tag(shown.name, x, y, {color = shown == self.selected and '#FFFFC93C' or Test.accent})
    end
    graphics2d.drawRing(self.cursorX, self.cursorY, 14, 3, '#E0FFFFFF', {layer = 100})
    graphics2d.drawCircle(self.cursorX, self.cursorY, 3, '#FFFFFFFF', {layer = 100})
    graphics2d.popClip()
end

return Picking
