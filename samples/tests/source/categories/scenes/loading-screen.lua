-- Loading screen that fades out: an island map that raises its terrain in its "load" hook behind a full-screen loading screen, which fades out over the finished map once the load is done and the screen stayed its minimum time. The map lists the events of the change in the order they came, so the right events show the right moments: the out half of the transition, the load on the covered screen, the fade of the loading screen and the in half.
local assets = require('haylen.assets')
local async = require('async')
local graphics = require('haylen.graphics')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local m = require('haylen.math')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Journal = require('harness.journal')
local SceneTest = require('categories.scenes.scene-test')
local Test = require('harness.test')

local LoadingScreen = haylen.class('LoadingScreen', SceneTest)

LoadingScreen.code = [[
local IslandMap = haylen.class('IslandMap', scene.Scene)
function IslandMap:load(context)
    context:progress(row / rows, 'Raising the island')  -- The loading screen shows it.
end
local options = {effect = 'fade', loading = TitleScreen(), minimumLoadingTime = 1.5, loadingFadeOut = 0.6}
scene.push(IslandMap(), options)]]

local kColumns, kRows = 192, 108
local kEvents = {'sceneLoading', 'sceneLoaded', 'transitionOutFinished', 'sceneEntered', 'transitionInStarted', 'transitionInFinished', 'sceneEnterTransitionFinished', 'appBackground', 'appActive'}
-- The colors of the map at rising heights, blended between their heights: deep and shallow water, the sand of the shore, grass, forest, rock and snow.
local kTerrain = {{-0.4, {0.11, 0.31, 0.48}}, {0, {0.18, 0.7, 0.77}}, {0.04, {0.95, 0.84, 0.55}}, {0.1, {0.48, 0.78, 0.3}}, {0.32, {0.23, 0.44, 0.22}}, {0.52, {0.49, 0.48, 0.45}}, {0.7, {0.95, 0.95, 0.97}}}
-- The walker sheet has cells of 256 pixels in eight columns, with the run in the second row.
local kRun = {8, 9, 10, 11, 12, 13, 14, 15}
local kTips = {'Tip: the fire keeps the night away.', 'Tip: sheep follow the one who feeds them.', 'Tip: the tide hides the reef at dusk.'}

-- The loading screen covers the whole screen with the title, the walker running toward the island, a bar with round ends and the message of the load, and the engine fades it out over the covered frame, so it needs no fade of its own.
local TitleScreen = haylen.class('TitleScreen')

function TitleScreen:enter()
    self.time = 0
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.walker = assets.texture('sprites/sheets/walker.png', {filter = 'linear'})
end

function TitleScreen:update(dt)
    self.time = self.time + dt
end

function TitleScreen:render(progress, message)
    graphics2d.beginScreen()
    self:paint(graphics2d.canvasBounds(), progress, message, 1, 0)
end

-- Paints the loading screen into `area` at `scale` times the size of its parts, with corners of `radius`, so the test shows a preview of it before the push.
function TitleScreen:paint(area, progress, message, scale, radius)
    local centerX = area.x + area.width / 2
    graphics2d.drawShape({area.x, area.y, area.width, area.height}, {radius = radius, color = '#FF1E2138', layer = 1})
    graphics2d.pushClip({area.x, area.y, area.width, area.height})
    graphics2d.drawShape({centerX - 520 * scale, area.y + area.height * 0.34 - 260 * scale, 1040 * scale, 520 * scale}, {kind = 'ellipse', color = '#402F86F6', softness = 260 * scale, layer = 1})
    graphics2d.popClip()
    graphics2d.drawText(self.font, 'Isle of Tests', centerX, area.y + area.height * 0.3, {size = 130 * scale, anchor = {0.5, 0.5}, outlineWidth = 8 * scale, outlineColor = '#FF15172A', shadowOffset = {0, 8 * scale}, shadowColor = '#80000000', layer = 2})

    local width = math.min(900 * scale, area.width - 120 * scale)
    local x, y = centerX - width / 2, area.y + area.height * 0.62
    local frame = kRun[math.floor(self.time * 14) % #kRun + 1]
    graphics2d.draw(self.walker, x + 20 * scale + (width - 40 * scale) * progress, y - 6 * scale, {source = {frame % 8 * 256, frame // 8 * 256, 256, 256}, width = 150 * scale, height = 150 * scale, pivotY = 248 / 256, layer = 3})
    graphics2d.drawShape({x, y, width, 34 * scale}, {radius = 17 * scale, color = '#FF15172A', borderWidth = 3 * scale, borderColor = '#FF3A4066', layer = 2})
    graphics2d.drawShape({x + 5 * scale, y + 5 * scale, math.max(24 * scale, (width - 10 * scale) * progress), 24 * scale}, {radius = 12 * scale, color = '#FFFFC93C', layer = 3})
    graphics2d.drawText(nil, message or '', centerX, y + 70 * scale, {size = 32 * scale, color = '#FFB9C0E0', anchor = {0.5, 0}, layer = 2})
    graphics2d.drawText(nil, kTips[math.floor(self.time / 2) % #kTips + 1], centerX, area.y + area.height - 80 * scale, {size = 28 * scale, color = '#FF8A90B0', anchor = {0.5, 1}, layer = 2})
end

-- The map raises its terrain from noise in steps that report their progress, and bakes it into a texture once, so drawing it costs one draw per frame.
local IslandMap = haylen.class('IslandMap', scene.Scene)

function IslandMap:init(journal)
    self.journal = journal
end

function IslandMap:load(context)
    local noise = m.noise(17)
    local image = graphics.newImage(kColumns, kRows)
    for row = 1, kRows do
        for column = 1, kColumns do
            local dx, dy = (column - kColumns / 2) / (kColumns / 2), (row - kRows / 2) / (kRows / 2)
            local falloff = math.sqrt(dx * dx + dy * dy)
            local height = noise:fractal(column / 36, row / 36, 5, 2, 0.5) * 0.9 + 0.35 - falloff * 0.75
            image:setPixel(column - 1, row - 1, self:colorOf(height))
        end
        if row % 12 == 0 then
            context:progress(row / kRows, string.format('Raising the island, row %d of %d', row, kRows))
            async.sleep(90):await()
        end
    end
    self.image = image
    context:progress(1, 'The island is ready')
end

function IslandMap:enter()
    self.map = graphics.newTexture(self.image, {filter = 'linear'})
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.gui = ui.mount(ui.column{padding = 32, onCancel = function() self:leave() end,
        ui.row{gap = 16, ui.button{id = 'back', text = 'Back to the test', onClick = function() self:leave() end}, ui.spacer{grow = 1}},
    }, {owner = self})
    self.gui:command('back', 'focus')
end

function IslandMap:leave()
    if scene.top() == self and not scene.transitioning() then
        scene.pop({effect = 'fade', duration = 0.4})
    end
end

function IslandMap:update()
    if input.pressed('back') then
        self:leave()
    end
end

-- Blends the colors of the two heights of the terrain around the height.
function IslandMap:colorOf(height)
    if height <= kTerrain[1][1] then
        return {kTerrain[1][2][1], kTerrain[1][2][2], kTerrain[1][2][3], 1}
    end
    for index = 2, #kTerrain do
        local below, above = kTerrain[index - 1], kTerrain[index]
        if height < above[1] then
            local t = (height - below[1]) / (above[1] - below[1])
            local a, b = below[2], above[2]
            return {a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t, a[3] + (b[3] - a[3]) * t, 1}
        end
    end
    local top = kTerrain[#kTerrain][2]
    return {top[1], top[2], top[3], 1}
end

-- Covers the screen with the map, smoothed by the linear filter, and lists the events of the change on a card over it.
function IslandMap:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local scale = math.max(area.width / kColumns, area.height / kRows)
    graphics2d.draw(self.map, area.x + area.width / 2, area.y + area.height / 2, {width = kColumns * scale, height = kRows * scale})
    local card = {area.x + 32, area.y + 150, 720, 560}
    graphics2d.drawShape(card, {radius = 24, color = '#E61E2138', borderWidth = 3, borderColor = '#FF3A4066', layer = 1})
    graphics2d.drawText(self.font, 'The events of the change', card[1] + 28, card[2] + 36, {size = 32, anchor = {0, 0.5}, outlineWidth = 3, outlineColor = '#FF15172A', layer = 2})
    self.journal:draw(card[1] + 8, card[2] + 76, card[4] - 92, 24, card[3] - 24)
end

function LoadingScreen:enter()
    self.journal = Journal(18)
    self.preview = TitleScreen()
    self.preview:enter()
    for _, name in ipairs(kEvents) do
        self:listen(name, function(value)
            if not self.started then
                return
            end
            self.journal:add(string.format('%.2f s: the event "%s"', haylen.elapsed() - self.started, name), name:match('^app') and Test.warm or Test.accent)
        end)
    end
    self:frame{
        hint = 'The loading screen appears at once, stays at least a second and a half and fades out over the finished map. Escape or the B button returns from the map.',
        code = LoadingScreen.code,
        controls = {
            ui.button{id = 'open', text = 'Open the island map', variant = 'primary', align = 'stretch', onClick = function() self:open() end},
        },
        focus = 'open',
    }
end

function LoadingScreen:open()
    if scene.transitioning() then
        return
    end
    self.journal:clear()
    self.started = haylen.elapsed()
    scene.push(IslandMap(self.journal), {effect = 'fade', duration = 0.6, color = '#FF0B1320', loading = TitleScreen(), loadingDelay = 0, minimumLoadingTime = 1.5, loadingFadeOut = 0.6})
end

function LoadingScreen:update(dt)
    LoadingScreen.super.update(self, dt)
    self.preview:update(dt)
end

-- Before the first push, the card shows the loading screen in small, with its bar filling in a loop.
function LoadingScreen:drawPreview(card)
    local width = math.min(card[3] - 80, (card[4] - 150) * 16 / 9)
    local height = width * 9 / 16
    local area = m.rect(card[1] + (card[3] - width) / 2, card[2] + 60, width, height)
    local progress = self.preview.time * 0.25 % 1
    self.preview:paint(area, progress, string.format('Raising the island, row %d of %d', math.floor(progress * kRows), kRows), width / 1400, 16)
    graphics2d.drawText(nil, 'Open the island map to watch this loading screen and the events of the change.', card[1] + card[3] / 2, area.y + height + 24, {size = 26, color = Test.muted, anchor = {0.5, 0}, maxWidth = card[3] - 80, align = 'center', layer = 2})
end

function LoadingScreen:draw(area)
    local card = {24, 24, area.width - 48, area.height - 48}
    graphics2d.drawShape(card, {radius = 18, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    if self.journal.lines.size == 0 then
        graphics2d.drawText(nil, 'The loading screen of the island map', card[1] + 20, card[2] + 18, {size = 22, color = Test.muted, layer = 2})
        self:drawPreview(card)
        return
    end
    graphics2d.drawText(nil, 'The events of the last change, in seconds since the push', card[1] + 20, card[2] + 18, {size = 22, color = Test.muted, layer = 2})
    self.journal:draw(card[1] + 4, card[2] + 60, card[4] - 76, 26, card[3] - 16)
end

return LoadingScreen
