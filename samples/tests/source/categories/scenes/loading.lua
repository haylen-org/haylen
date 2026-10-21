-- Loading and errors: every scene loads before it enters. A fade holds its covered frame while the next scene loads, a loading view shows the progress the load reports, `scene.preload` loads a scene in the background, and `onError` receives a load that failed.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')
local Scenery = require('categories.effects.scenery')

local Loading = haylen.class('Loading', SceneTest)

Loading.code = [[
scene.push(card, {effect = 'iris', duration = 0.8, loading = LoadingView(), loadingDelay = 0.2, minimumLoadingTime = 0.6})
scene.spawn(self, function()
    if scene.preload(card, 'preloaded'):await() then scene.push(card, {effect = 'crossFade', duration = 0.5}) end
end)
scene.push(broken, {effect = 'fade', duration = 0.6, onError = function(message) scene.push(ErrorCard(message)) end})]]

-- The custom loading view: a plaque that slides up from the bottom with the title of the island, a bar with round ends in its groove, the percentage and the message of the load.
local LoadingView = haylen.class('LoadingView')

function LoadingView:enter()
    self.shown = 0
    self.font = assets.font('fonts/lilita-one-regular.ttf')
end

function LoadingView:update(dt, progress, message)
    self.shown = math.min(1, self.shown + dt * 4)
end

function LoadingView:renderUi(progress, message)
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local width = math.min(960, area.width - 96)
    local x, y = area.x + (area.width - width) / 2, area:bottom() - 300 + (1 - self.shown) * 140
    graphics2d.drawRect(area, {0.03, 0.04, 0.08, 0.55 * self.shown})
    graphics2d.drawShape({x, y + 10, width, 230}, {radius = 32, color = '#66000000', softness = 24, layer = 1})
    graphics2d.drawShape({x, y, width, 230}, {radius = 30, color = '#F22F3352', borderWidth = 4, borderColor = '#FFFFC93C', layer = 1})
    graphics2d.drawText(self.font, 'Isle of Tests', x + 40, y + 52, {size = 48, anchor = {0, 0.5}, outlineWidth = 4, outlineColor = '#FF15172A', layer = 2})
    graphics2d.drawText(self.font, string.format('%d%%', math.floor(progress * 100)), x + width - 40, y + 52, {size = 48, color = '#FFFFC93C', anchor = {1, 0.5}, outlineWidth = 4, outlineColor = '#FF15172A', layer = 2})
    local bar = {x + 40, y + 104, width - 80, 34}
    graphics2d.drawShape(bar, {radius = 17, color = '#FF15172A', layer = 2})
    graphics2d.drawShape({bar[1] + 4, bar[2] + 4, math.max(26, (bar[3] - 8) * progress), 26}, {radius = 13, color = '#FF2F86F6', layer = 3})
    graphics2d.drawText(nil, message or '', x + 40, y + 180, {size = 28, color = '#FFB9C0E0', anchor = {0, 0.5}, layer = 2})
end

function Loading:init(entry)
    Loading.super.init(self, entry)
    self.count = 0
    self.scenery = Scenery()
end

function Loading:enter()
    self:frame{
        hint = 'Each card takes about two seconds to load. Escape or the B button pops a card, and cards pop by themselves after a moment.',
        code = Loading.code,
        view = Card.world,
        controls = {
            ui.button{id = 'fade', text = 'Behind the fade', align = 'stretch', onClick = function() self:fade() end},
            ui.button{id = 'view', text = 'With a loading view', align = 'stretch', onClick = function() self:withView() end},
            ui.button{id = 'preload', text = 'Preload in the background', align = 'stretch', onClick = function() self:preload() end},
            ui.button{id = 'failing', text = 'A load that fails', align = 'stretch', onClick = function() self:failing() end},
        },
        focus = 'fade',
    }
    self:report('Pick a way to load the island at night.')
end

function Loading:card(title, options)
    self.count = self.count + 1
    options = options or {}
    options.title, options.night, options.stay = title, true, 1.5
    options.caption = options.caption or 'Loaded, entered and shown'
    options.work = options.work or 2
    options.leave = {effect = 'fade', duration = 0.4}
    return Card(options)
end

function Loading:report(text)
    self:set('status', {text = text})
end

-- Without a loading view the fade is the loading screen: it holds its covered frame until the card loaded.
function Loading:fade()
    if not scene.transitioning() then
        scene.push(self:card('Behind the fade'), {effect = 'fade', duration = 0.8, color = '#FF101418'})
        self:report('The fade holds its covered frame while the card loads')
    end
end

-- The loading view appears when the load takes longer than the delay and stays at least its minimum time.
function Loading:withView()
    if not scene.transitioning() then
        scene.push(self:card('With a view'), {effect = 'iris', duration = 0.8, loading = LoadingView(), loadingDelay = 0.2, minimumLoadingTime = 0.6, params = 'A custom loading view'})
        self:report('The loading view shows the progress the card reports')
    end
end

-- The card loads while this scene keeps running and drawing its progress, and the push that follows takes it at once.
function Loading:preload()
    if self.preloading or scene.transitioning() then
        return
    end
    local card = self:card('Preloaded', {caption = 'It loaded while the test kept running'})
    self.preloading = card
    self:set('preload', {enabled = false})
    self:spawn(function()
        local loaded = scene.preload(card, 'preloaded'):await()
        self.preloading = nil
        self:set('preload', {enabled = true})
        if loaded then
            scene.push(card, {effect = 'crossFade', duration = 0.5})
        end
    end)
end

-- The load fails halfway, the test stays on top, and `onError` receives the message and routes to a card that shows it.
function Loading:failing()
    if not scene.transitioning() then
        local broken = self:card('Broken', {fail = 'The save file is corrupt.'})
        scene.push(broken, {effect = 'fade', duration = 0.6, loading = LoadingView(), onError = function(message)
            self:report('The handler "onError" received "' .. message .. '"')
            scene.push(self:card('Load failed', {caption = message, work = 0}), {effect = 'fade', duration = 0.4})
        end})
    end
end

function Loading:update(dt)
    Loading.super.update(self, dt)
    self.scenery:update(dt)
end

-- Draws the island by day, and while a card preloads, a plaque with its progress over it.
function Loading:draw(area)
    self.scenery:draw(false)
    if not self.preloading then
        return
    end
    local progress, message = scene.loadProgress(self.preloading)
    local bounds = graphics2d.canvasBounds()
    local width = math.min(1100, bounds.width - 160)
    local x, y = -width / 2, -360
    graphics2d.drawShape({x - 40, y - 70, width + 80, 190}, {radius = 30, color = '#E62F3352', borderWidth = 4, borderColor = '#FFFFC93C', layer = 1})
    graphics2d.drawText(nil, string.format('Preloading %d%%: %s', math.floor(progress * 100), message or ''), x, y - 20, {size = 36, color = '#FFFFFFFF', anchor = {0, 0.5}, layer = 2})
    graphics2d.drawShape({x, y + 30, width, 34}, {radius = 17, color = '#FF15172A', layer = 2})
    graphics2d.drawShape({x + 4, y + 34, math.max(26, (width - 8) * progress), 26}, {radius = 13, color = '#FFFFC93C', layer = 3})
end

return Loading
