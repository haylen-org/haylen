-- Loading screen that fades out: an island map that raises its terrain in its "load" hook behind a full-screen loading screen, which fades out over the finished map once the load is done and the screen stayed its minimum time. The map lists the events of the change in the order they came, so the right events show the right moments: the load, the hold, the fade of the loading screen and the reveal.
local async = require('async')
local events = require('haylen.events')
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

local kColumns, kRows, kCell = 96, 54, 20
local kEvents = {'sceneLoading', 'sceneLoaded', 'sceneHoldStarted', 'sceneHoldFinished', 'sceneEntered', 'sceneRevealStarted', 'sceneRevealFinished', 'sceneEnterTransitionFinished', 'appBackground', 'appActive'}
local kTerrain = {{-1, '#FF1B4F7A'}, {0.02, '#FF2E6F9E'}, {0.08, '#FFE3D19A'}, {0.22, '#FF5E9C4A'}, {0.42, '#FF3B7038'}, {0.6, '#FF7C7A72'}, {2, '#FFF0F0F0'}}
local kTips = {'Tip: the fire keeps the night away.', 'Tip: sheep follow the one who feeds them.', 'Tip: the tide hides the reef at dusk.'}

-- The loading screen covers the whole screen with the title, a bar and the message of the load, and the engine fades it out over the covered frame, so it needs no fade of its own.
local TitleScreen = haylen.class('TitleScreen')

function TitleScreen:enter()
    self.time = 0
end

function TitleScreen:update(dt)
    self.time = self.time + dt
end

function TitleScreen:render(progress, message)
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF0B1320')
    for index = 0, 11 do
        local angle = self.time * 0.6 + index * math.pi / 6
        graphics2d.drawCircle(area.x + area.width / 2 + math.cos(angle) * 220, area.y + area.height * 0.36 + math.sin(angle) * 220, 14, '#604C7DFF', {layer = 1})
    end
    graphics2d.drawText(nil, 'Isle of Tests', area.x + area.width / 2, area.y + area.height * 0.36, {size = 110, anchor = {0.5, 0.5}, outlineWidth = 4, outlineColor = '#FF05080F', layer = 2})
    local width = math.min(900, area.width - 120)
    local x, y = area.x + (area.width - width) / 2, area.y + area.height * 0.66
    graphics2d.drawRect({x, y, width, 22}, '#FF26304A', {layer = 2})
    graphics2d.drawRect({x, y, width * progress, 22}, '#FFF2C14E', {layer = 3})
    graphics2d.drawText(nil, message or '', area.x + area.width / 2, y + 60, {size = 32, color = '#FFA3A8BF', anchor = {0.5, 0}, layer = 2})
    graphics2d.drawText(nil, kTips[math.floor(self.time / 2) % #kTips + 1], area.x + area.width / 2, area:bottom() - 80, {size = 28, color = '#FF7A8099', anchor = {0.5, 1}, layer = 2})
end

-- The map raises its terrain from noise in steps that report their progress, and bakes it into a texture once, so drawing it costs one draw per frame.
local IslandMap = haylen.class('IslandMap', scene.Scene)

function IslandMap:init(journal)
    self.journal = journal
end

function IslandMap:load(context)
    local noise = m.noise(17)
    self.heights = {}
    for row = 1, kRows do
        local line = {}
        for column = 1, kColumns do
            local dx, dy = (column - kColumns / 2) / (kColumns / 2), (row - kRows / 2) / (kRows / 2)
            local falloff = math.sqrt(dx * dx + dy * dy)
            line[column] = noise:fractal(column / 18, row / 18, 5, 2, 0.5) * 0.9 + 0.35 - falloff * 0.75
        end
        self.heights[row] = line
        if row % 6 == 0 then
            context:progress(row / kRows, string.format('Raising the island, row %d of %d', row, kRows))
            async.sleep(90):await()
        end
    end
    context:progress(1, 'The island is ready')
end

function IslandMap:enter()
    self.target = graphics.newRenderTarget(kColumns * kCell, kRows * kCell)
    self.camera = graphics2d.newCamera()
    self.camera.x, self.camera.y = kColumns * kCell / 2, kRows * kCell / 2
    self.baked = false
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

function IslandMap:colorOf(height)
    for _, band in ipairs(kTerrain) do
        if height < band[1] then
            return band[2]
        end
    end
    return kTerrain[#kTerrain][2]
end

function IslandMap:render()
    if not self.baked then
        self.baked = true
        graphics2d.beginTarget(self.target, self.camera, {clear = '#FF1B4F7A'})
        for row = 1, kRows do
            for column = 1, kColumns do
                graphics2d.drawRect({(column - 1) * kCell, (row - 1) * kCell, kCell, kCell}, self:colorOf(self.heights[row][column]))
            end
        end
    end
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local scale = math.max(area.width / self.target.width, area.height / self.target.height)
    graphics2d.draw(self.target.texture, area.x + area.width / 2, area.y + area.height / 2, {width = self.target.width * scale, height = self.target.height * scale})
    graphics2d.drawRect({area.x + 32, area.y + 140, 760, 560}, '#C0101418', {layer = 1})
    self.journal:draw(area.x + 56, area.y + 160, 520, 26)
end

function LoadingScreen:enter()
    self.journal = Journal(18)
    for _, name in ipairs(kEvents) do
        self:listen(name, function(value)
            self.journal:add(string.format('%6.2f s  Event "%s"', haylen.elapsed() - (self.started or haylen.elapsed()), name), name:match('^app') and Test.warm or Test.accent)
        end)
    end
    self:frame{
        hint = 'The loading screen appears at once, stays at least a second and a half and fades out over the finished map. Escape or the B button returns from the map.',
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

function LoadingScreen:draw(area)
    graphics2d.drawRect(area, '#FF141B28')
    self.journal:draw(24, 16, area.height - 32, 26)
end

return LoadingScreen
