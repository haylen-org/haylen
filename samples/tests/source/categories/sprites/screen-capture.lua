-- Screen capture: the button captures the next frame with everything the screen shows, the GPU copies it without the frame thread waiting, and the test shows the image in a frame next to the scene, writes it as a PNG file to the private storage of the app and reads it back.
local assets = require('haylen.assets')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local storage = require('haylen.storage')
local ui = require('haylen.ui')

local Panels = require('categories.sprites.panels')
local SpriteTest = require('categories.sprites.sprite-test')
local Terrain = require('categories.sprites.terrain')
local Test = require('harness.test')

local ScreenCapture = haylen.class('ScreenCapture', SpriteTest)

ScreenCapture.path = 'tests/screen-capture.png'
ScreenCapture.rows = 8
ScreenCapture.coins = {{2, 398, 128, 128}, {134, 398, 128, 128}, {266, 398, 128, 128}, {398, 398, 128, 128}, {266, 398, 128, 128}, {134, 398, 128, 128}}
ScreenCapture.slime = {{530, 398, 128, 128}, {662, 398, 128, 128}}
ScreenCapture.layouts = {
    landscape = {view = {1700, 800}, cards = {scene = {-830, -380, 820, 760}, shot = {10, -380, 820, 760}}},
    portrait = {view = {900, 1700}, cards = {scene = {-430, -830, 860, 820}, shot = {-430, 10, 860, 820}}},
}

ScreenCapture.code = [[
local shot = graphics.captureScreen():await()
local png = shot:encodePngAsync():await()
storage.writeTextAsync('tests/screen-capture.png', png):await()
local back = graphics.decodeImage(storage.readText('tests/screen-capture.png'))]]

function ScreenCapture:enter()
    self.time = 0
    self.message = 'Press "Capture the screen" to take a picture of everything the screen shows.'
    self.hero = SpriteTest.texture('images/hero.png')
    self.terrain = assets.texture('tiled/tiles/terrain.png', Terrain.textureOptions)
    self.props = assets.texture('tiled/tiles/props.png', Terrain.textureOptions)
    self.layout = ScreenCapture.layouts.landscape
    self:frame{
        code = ScreenCapture.code,
        view = self.layout.view,
        hint = 'Capture the screen, which comes back as an image a frame later, shown in the frame on the right and saved as a PNG file.',
        controls = {ui.button{id = 'capture', text = 'Capture the screen', variant = 'primary', onClick = function() self:capture() end}},
        focus = 'capture',
    }
end

-- Picks the layout that draws the cards larger in the play area and bakes the island of the scene: grass around a pond with a sandy shore.
function ScreenCapture:resize(area)
    local layout = Panels.layout(ScreenCapture.layouts, area)
    self.view = layout.view
    if layout == self.layout and self.grass then
        return
    end
    self.layout = layout
    local rect = layout.cards.scene
    local rows = ScreenCapture.rows
    local size = (rect[4] - 86) / rows
    local columns = math.floor((rect[3] - 40) / size)
    self.map = {rect[1] + (rect[3] - columns * size) / 2, rect[2] + 66, columns * size, rows * size}
    self.cell = size
    self.grass, self.edges = Terrain.bake(columns, rows, size, self.map[1], self.map[2], function(column, row)
        local dx, dy = (column - columns / 2) / (columns * 0.22), (row - rows / 2) / (rows * 0.21)
        local pond = dx * dx + dy * dy
        if pond < 1 then
            return Terrain.water
        end
        return pond < 2.2 and Terrain.sand or Terrain.grass
    end)
end

function ScreenCapture:capture()
    scene.spawn(self, function()
        local started = haylen.frameIndex()
        local shot = graphics.captureScreen():await()
        local frames = haylen.frameIndex() - started
        local png = shot:encodePngAsync():await()
        storage.writeTextAsync(ScreenCapture.path, png):await()
        local back = graphics.decodeImage(storage.readText(ScreenCapture.path))
        self.shot = graphics.newTexture(shot, {filter = 'linear', mipmaps = true})
        self.message = string.format('Captured %d by %d pixels %d %s later, saved %.0f KB as a PNG file and read it back at %d by %d.', shot.width, shot.height, frames, frames == 1 and 'frame' or 'frames', #png / 1024, back.width, back.height)
    end)
end

function ScreenCapture:update(dt)
    ScreenCapture.super.update(self, dt)
    self.time = self.time + dt
    self:status(self.message)
end

-- The island moves, so every capture differs: the hero walks around the pond, coins spin and a slime hops.
function ScreenCapture:drawScene(rect)
    Panels.card(rect, 'A scene to capture')
    local map, cell, time = self.map, self.cell, self.time
    graphics2d.pushMask(function()
        graphics2d.drawShape(map, {radius = 14})
    end)
    graphics2d.drawStatic(self.grass, 0, 0)
    graphics2d.drawStatic(self.edges, 0, 0, {layer = 1})
    local centerX, centerY = map[1] + map[3] / 2, map[2] + map[4] / 2
    local things = {
        {texture = self.props, source = {2, 2, 128, 256}, x = map[1] + cell * 1.2, y = map[2] + cell * 2.6, height = cell * 2.4},
        {texture = self.props, source = {134, 2, 128, 256}, x = map[1] + map[3] - cell * 1.1, y = map[2] + cell * 2.2, height = cell * 2.4},
        {texture = self.props, source = {2, 2, 128, 256}, x = map[1] + map[3] - cell * 1.6, y = map[2] + map[4] - cell * 0.3, height = cell * 2.2},
        {texture = self.terrain, source = {266, 266, 128, 128}, x = map[1] + cell * 1.6, y = map[2] + map[4] - cell * 0.6, height = cell},
    }
    for index = 0, 4 do
        local frame = ScreenCapture.coins[math.floor(time * 10 + index * 2) % #ScreenCapture.coins + 1]
        local angle = index * math.pi * 2 / 5 + 0.4
        things[#things + 1] = {texture = self.terrain, source = frame, x = centerX + math.cos(angle) * cell * 3.4, y = centerY + math.sin(angle) * cell * 2.7 + cell * 0.25, height = cell * 0.55}
    end
    local hop = math.abs(math.sin(time * 4))
    things[#things + 1] = {texture = self.terrain, source = ScreenCapture.slime[hop > 0.5 and 1 or 2], x = map[1] + cell * 2.6, y = map[2] + cell * 5.6 - hop * cell * 0.3, height = cell * 0.8}
    local angle = time * 0.5
    things[#things + 1] = {texture = self.hero, x = centerX + math.cos(angle) * cell * 3.9, y = centerY + math.sin(angle) * cell * 3 + cell * 0.6 - math.abs(math.sin(time * 8)) * 6, height = cell * 1.3, flip = math.sin(angle) > 0}
    table.sort(things, function(a, b) return a.y < b.y end)
    for _, thing in ipairs(things) do
        local width = thing.source and thing.height * thing.source[3] / thing.source[4] or thing.height
        graphics2d.draw(thing.texture, thing.x, thing.y, {source = thing.source, width = width, height = thing.height, pivotY = 0.94, flipHorizontal = thing.flip, layer = 2})
    end
    graphics2d.popMask()
end

-- The frame of the capture waits for the first picture with an empty frame and a camera, and then shows the picture at its own shape.
function ScreenCapture:drawShot(rect)
    local inner = Panels.card(rect, 'The capture')
    local x, y = inner[1] + inner[3] / 2, inner[2] + inner[4] / 2
    if not self.shot then
        graphics2d.drawShape({inner[1] + 20, inner[2] + 20, inner[3] - 40, inner[4] - 40}, {radius = 16, color = '#FF181D2A', borderWidth = 3, borderColor = Test.line})
        graphics2d.drawShape({x - 40, y - 74, 80, 56}, {radius = 12, color = '#00000000', borderWidth = 5, borderColor = Test.muted})
        graphics2d.drawShape({x - 16, y - 62, 32, 32}, {kind = 'ellipse', color = '#00000000', borderWidth = 5, borderColor = Test.muted})
        graphics2d.drawShape({x - 26, y - 84, 22, 12}, {radius = 4, color = Test.muted})
        Panels.caption('The capture shows here', x, y + 4, {color = Test.muted})
        return
    end
    local width = math.min(inner[3] - 40, (inner[4] - 40) * self.shot.width / self.shot.height)
    local height = width * self.shot.height / self.shot.width
    graphics2d.drawShape({x - width / 2 - 10, y - height / 2 - 10, width + 20, height + 20}, {radius = 10, color = '#FFF4F1E8', layer = 1})
    graphics2d.draw(self.shot, x, y, {width = width, height = height, layer = 2})
end

function ScreenCapture:draw(area)
    local cards = self.layout.cards
    self:drawScene(cards.scene)
    self:drawShot(cards.shot)
end

return ScreenCapture
