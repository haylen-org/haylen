-- Native banner: a native view that the plugin places over the app through its overlay, at the top or the bottom of the safe area. While it reserves its edge the safe area shrinks, so this frame, which the UI lays out in the safe area, moves out of its way. Its native button sends bannerTapped, and taps anywhere else reach the app, which counts the ones on the stage.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local demo = require('native-demo')
local sample = require('sample')

local Banner = haylen.class('Banner', sample.Test)

local kInsetsInterval = 0.25

function Banner:enter()
    self.anchor = 'bottom'
    self.reserve = true
    self.shown = false
    self.visible = true
    self.gameTaps = 0
    self.insetsTime = 0
    self.connection = demo.onBannerTapped(function(payload)
        self.results:set('taps', 'pass', 'the native button reaches Lua', string.format('%s sent bannerTapped %d times.', payload.language, payload.count))
    end)
    self:frame({
        hint = 'Show the banner, tap its button and tap the stage around it.',
        focus = 'show',
        controls = {
            ui.button{id = 'show', text = 'Show the banner', variant = 'primary', onClick = function() self:place(self.anchor, self.reserve) end},
            ui.button{id = 'anchor', text = 'Move it to the top', onClick = function() self:place(self.anchor == 'bottom' and 'top' or 'bottom', self.reserve) end},
            ui.button{id = 'reserve', text = 'Stop reserving its edge', onClick = function() self:place(self.anchor, not self.reserve) end},
            ui.button{id = 'visible', text = 'Hide it', onClick = function() self:toggleVisible() end},
            ui.button{id = 'remove', text = 'Remove it', onClick = function() self:remove() end},
            ui.label{text = 'The overlay lets touches and clicks through everywhere but on the banner. Views of the overlay never take the focus, so the remote of a TV and gamepads keep driving the app and cannot press the native button.', color = 'textMuted', font = 'caption'},
            ui.label{font = 'monospace', text = "demo.showBanner('bottom', true)\ndemo.onBannerTapped(function(tap)\n  print(tap.count)\nend)\nprint(viewport.reservedInsets().bottom)"},
        },
    })
    if self.native then
        self.results:set('banner', 'info', 'the banner', 'Show the banner to place it.')
        self.results:set('taps', 'waiting', 'the native button reaches Lua', 'Tap the button of the banner.')
        self.results:set('game', 'waiting', 'other taps reach the app', 'Tap or click the stage.')
    end
end

function Banner:exit()
    self.connection:disconnect()
    if self.shown then
        demo.removeBanner()
    end
end

function Banner:place(anchor, reserve)
    self:act(function()
        local state, err = demo.showBanner(anchor, reserve):await()
        if err then
            self.results:failure('banner', 'the banner', err)
            if err.code == 'unsupported' then
                self.results:set('taps', 'skip', 'the native button reaches Lua', 'The platform shows no banner.')
            end
            return
        end
        self.anchor, self.reserve, self.shown, self.visible = state.anchor, state.reserve, true, state.visible
        self:showState()
    end)
end

function Banner:toggleVisible()
    self:act(function()
        local state, err = demo.setBannerVisible(not self.visible):await()
        if err then
            self.results:failure('banner', 'the banner', err)
            return
        end
        self.visible = state.visible
        self:showState()
    end)
end

function Banner:remove()
    self:act(function()
        local _, err = demo.removeBanner():await()
        if err then
            self.results:failure('banner', 'the banner', err)
            return
        end
        self.shown = false
        self:showState()
    end)
end

function Banner:showState()
    local text = 'The banner is removed.'
    if self.shown then
        text = string.format('The banner is %s at the %s and %s its edge.', self.visible and 'visible' or 'hidden', self.anchor, self.reserve and 'reserves' or 'does not reserve')
    end
    self.results:set('banner', 'info', 'the banner', text)
    self:set('anchor', {text = self.anchor == 'bottom' and 'Move it to the top' or 'Move it to the bottom'})
    self:set('reserve', {text = self.reserve and 'Stop reserving its edge' or 'Reserve its edge'})
    self:set('visible', {text = self.visible and 'Hide it' or 'Show it again'})
end

function Banner:update(dt)
    Banner.super.update(self, dt)
    self:countGameTaps()

    self.insetsTime = self.insetsTime + haylen.unscaledDelta()
    if self.native and self.insetsTime >= kInsetsInterval then
        self.insetsTime = 0
        local reserved = viewport.reservedInsets()
        local safe = viewport.safeRect()
        self.results:set('insets', 'info', 'the safe area', string.format('viewport.reservedInsets() top %.0f bottom %.0f, and the safe area runs from y %.0f to %.0f, where the UI lays out.', reserved.top, reserved.bottom, safe.y, safe.y + safe.height))
    end
end

-- Presses of the mouse and fingers that land on the stage reached the app through the overlay.
function Banner:countGameTaps()
    if not self.native or not self.stage then
        return
    end
    local points = {}
    for _, touch in ipairs(input.touches()) do
        if touch.phase == 'began' then
            points[#points + 1] = {touch.x, touch.y}
        end
    end
    if #points == 0 and input.mousePressed() then
        points[1] = {input.mousePosition()}
    end
    for _, point in ipairs(points) do
        local x, y = point[1] - self.stage.x, point[2] - self.stage.y
        if x >= 0 and y >= 0 and x <= self.stage.width and y <= self.stage.height then
            self.gameTaps = self.gameTaps + 1
            self.lastTap = {x, y}
            self.results:set('game', 'pass', 'other taps reach the app', string.format('The app received %d taps on the stage.', self.gameTaps))
        end
    end
end

function Banner:draw(area)
    if self.lastTap then
        graphics2d.drawCircle(self.lastTap[1], self.lastTap[2], 18, sample.warm, {layer = 3})
    end
end

return Banner
