-- Loading and errors: every scene loads before it enters. A fade holds its covered frame while the next scene loads, a loading view shows the progress the load reports, scene.preload loads a scene in the background, and onError receives a load that failed.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local scene = require('haylen.scene')
local ui = require('haylen.ui')

local Card = require('card')
local sample = require('sample')

local Loading = haylen.class('Loading', sample.Test)

Loading.hints = 'Each card takes about two seconds to load. Escape or the B button pops a card, and cards pop by themselves after a moment.'

-- The custom loading view: a panel with a bar, the percentage and the message of the load, sliding in when it appears.
local LoadingView = haylen.class('LoadingView')

function LoadingView:enter()
    self.shown = 0
end

function LoadingView:update(dt, progress, message)
    self.shown = math.min(1, self.shown + dt * 4)
end

function LoadingView:renderUi(progress, message)
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    local x, y = area.x + area.width / 2 - 450, area:bottom() - 260 + (1 - self.shown) * 120
    graphics2d.drawRect(area, {0.03, 0.04, 0.08, 0.6 * self.shown})
    graphics2d.drawRect({x - 30, y - 90, 960, 190}, '#F0232739', {layer = 1})
    graphics2d.drawText(nil, string.format('Loading %d%%', math.floor(progress * 100)), x, y - 70, {size = 44, layer = 2})
    graphics2d.drawRect({x, y, 900, 28}, '#FF3A4058', {layer = 2})
    graphics2d.drawRect({x, y, 900 * progress, 28}, '#FF4C7DFF', {layer = 3})
    graphics2d.drawText(nil, message or '', x, y + 44, {size = 30, color = '#FFA3A8BF', layer = 2})
end

function Loading:init(entry)
    Loading.super.init(self, entry)
    self.count = 0
end

function Loading:controls()
    return {
        ui.button{text = 'Behind the fade', align = 'stretch', onClick = function() self:fade() end},
        ui.button{text = 'With a loading view', align = 'stretch', onClick = function() self:view() end},
        ui.button{id = 'preload', text = 'Preload in the background', align = 'stretch', onClick = function() self:preload() end},
        ui.button{text = 'A load that fails', align = 'stretch', onClick = function() self:failing() end},
    }
end

function Loading:card(title, color, options)
    self.count = self.count + 1
    options = options or {}
    options.title, options.color, options.stay = title, color, 1.5
    options.caption = options.caption or 'loaded, entered and shown'
    options.work = options.work or 2
    options.leave = {effect = 'fade', duration = 0.4}
    return Card(options)
end

-- Without a loading view the fade is the loading screen: it holds its covered frame until the card loaded.
function Loading:fade()
    if not scene.transitioning() then
        scene.push(self:card('Behind the fade', '#FF2E5E8A'), {effect = 'fade', duration = 0.8, color = '#FF101418'})
        self:setStatus('the fade holds its covered frame while the card loads')
    end
end

-- The loading view appears when the load takes longer than the delay and stays at least its minimum time.
function Loading:view()
    if not scene.transitioning() then
        scene.push(self:card('With a view', '#FF3E8A5E'), {effect = 'iris', duration = 0.8, loading = LoadingView(), loadingDelay = 0.2, minimumLoadingTime = 0.6, params = 'a custom loading view'})
        self:setStatus('the loading view shows the progress the card reports')
    end
end

-- The card loads while this scene keeps running and drawing its progress, and the push that follows takes it at once.
function Loading:preload()
    if self.preloading or scene.transitioning() then
        return
    end
    local card = self:card('Preloaded', '#FF8A6E2E', {caption = 'it loaded while the test kept running'})
    self.preloading = card
    self.header:set('preload', {enabled = false})
    self:spawn(function()
        local loaded = scene.preload(card, 'preloaded'):await()
        self.preloading = nil
        self.header:set('preload', {enabled = true})
        if loaded then
            scene.push(card, {effect = 'crossFade', duration = 0.5})
        end
    end)
end

-- The load fails halfway, the test stays on top, and onError receives the message and routes to a card that shows it.
function Loading:failing()
    if not scene.transitioning() then
        local broken = self:card('Broken', '#FF5E3E8A', {fail = 'the save file is corrupt'})
        scene.push(broken, {effect = 'fade', duration = 0.6, loading = LoadingView(), onError = function(message)
            self:setStatus('onError received: ' .. message)
            scene.push(self:card('Load failed', '#FF8A3E3E', {caption = message, work = 0}), {effect = 'fade', duration = 0.4})
        end})
    end
end

function Loading:pause()
    self.header.visible = false
end

function Loading:resume()
    self.header.visible = true
end

-- A failed load keeps this scene on top and reveals it again without a resume, so the header shows again here too.
function Loading:enterTransitionFinished()
    self.header.visible = true
end

function Loading:render()
    graphics2d.beginScreen()
    local area = graphics2d.canvasBounds()
    graphics2d.drawRect(area, '#FF1C2230')
    local time = haylen.elapsed()
    for index = 0, 9 do
        local x = area.x + area.width * (index + 0.5) / 10
        graphics2d.drawCircle(x, area.y + area.height * 0.7 + math.sin(time * 3 + index * 0.6) * 60, 26, '#FF4C7DFF')
    end
    if self.preloading then
        local progress, message = scene.loadProgress(self.preloading)
        graphics2d.drawRect({area.x + 200, area.y + 520, 900, 24}, '#FF3A4058')
        graphics2d.drawRect({area.x + 200, area.y + 520, 900 * progress, 24}, '#FFF2C14E', {layer = 1})
        graphics2d.drawText(nil, string.format('preload %d%%, %s', math.floor(progress * 100), message or ''), area.x + 200, area.y + 470, {size = 32, layer = 1})
    end
end

return Loading
