-- A native view that the plugin places over the app through its overlay, at the top or the bottom of the safe area. While it reserves its edge the safe area shrinks, so the frame of the test, which the UI lays out in the safe area, moves out of its way. Its native button sends `bannerTapped`, and taps anywhere else reach the app, which counts the ones on the tap zone with a ripple where each one lands.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local DemoTest = require('categories.plugins.demo-test')
local Test = require('harness.test')
local demo = require('native-demo')

local Banner = haylen.class('Banner', DemoTest)

Banner.insetsInterval = 0.25
Banner.rippleSeconds = 0.6

function Banner:enter()
    self.anchor = 'bottom'
    self.reserve = true
    self.shown = false
    self.visible = true
    self.gameTaps = 0
    self.ripples = {}
    self.insetsTime = 0
    self.connection = demo.onBannerTapped(function(payload)
        self.results:set('taps', 'pass', 'The native button reaches Lua', string.format('%s sent "bannerTapped" %d %s.', payload.language, payload.count, payload.count == 1 and 'time' or 'times'))
    end)
    self:frame{
        hint = 'Show the banner, tap its button and tap the zone around it.',
        focus = 'show',
        controls = {
            ui.button{id = 'show', text = 'Show the banner', variant = 'primary', onClick = function() self:place(self.anchor, self.reserve) end},
            ui.button{id = 'anchor', text = 'Move it to the top', onClick = function() self:place(self.anchor == 'bottom' and 'top' or 'bottom', self.reserve) end},
            ui.button{id = 'reserve', text = 'Stop reserving its edge', onClick = function() self:place(self.anchor, not self.reserve) end},
            ui.button{id = 'visible', text = 'Hide it', onClick = function() self:toggleVisible() end},
            ui.button{id = 'remove', text = 'Remove it', onClick = function() self:remove() end},
            ui.label{text = 'The overlay lets touches and clicks through everywhere but on the banner. Views of the overlay never take the focus, so the remote of a TV and gamepads keep driving the app.', color = 'textMuted', font = 'caption'},
        },
        showcase = {
            ui.card{align = 'stretch', gap = 8,
                ui.row{gap = 12, ui.label{text = 'The banner', font = 'heading', grow = 1}, ui.statusIndicator{id = 'bannerState', text = 'Not shown', tone = 'information'}},
                ui.label{id = 'safeArea', text = '', color = 'textMuted'},
            },
            ui.playArea{id = 'stage', grow = 1, align = 'stretch'},
        },
        code = "demo.showBanner('bottom', true)\ndemo.onBannerTapped(function(tap)\n  print(tap.count)\nend)\nprint(viewport.reservedInsets().bottom)",
    }
    if self.native then
        self.results:set('taps', 'waiting', 'The native button reaches Lua', 'Tap the button of the banner.')
        self.results:set('game', 'waiting', 'Other taps reach the app', 'Tap or click the zone next to the checks.')
    end
end

function Banner:exit()
    self.connection:disconnect()
    if self.shown then
        demo.removeBanner()
    end
    Banner.super.exit(self)
end

function Banner:place(anchor, reserve)
    self:act(function()
        local state, err = demo.showBanner(anchor, reserve):await()
        if err then
            self:set('bannerState', {text = 'Not shown here', tone = 'danger'})
            self.results:failure('banner', 'The banner shows', err)
            if err.code == 'unsupported' then
                self.results:set('taps', 'skip', 'The native button reaches Lua', 'The platform shows no banner.')
            end
            return
        end
        self.anchor, self.reserve, self.shown, self.visible = state.anchor, state.reserve, true, state.visible
        self.results:set('banner', 'pass', 'The banner shows', string.format('The plugin placed it at the %s of the safe area.', state.anchor))
        self:showState()
    end)
end

function Banner:toggleVisible()
    self:act(function()
        local state, err = demo.setBannerVisible(not self.visible):await()
        if err then
            self.results:failure('banner', 'The banner shows', err)
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
            self.results:failure('banner', 'The banner shows', err)
            return
        end
        self.shown = false
        self:showState()
    end)
end

function Banner:showState()
    local text, tone = 'Removed', 'information'
    if self.shown then
        text = string.format('%s at the %s, %s its edge', self.visible and 'Shown' or 'Hidden', self.anchor, self.reserve and 'reserving' or 'not reserving')
        tone = self.visible and 'success' or 'warning'
    end
    self:set('bannerState', {text = text, tone = tone})
    self:set('anchor', {text = self.anchor == 'bottom' and 'Move it to the top' or 'Move it to the bottom'})
    self:set('reserve', {text = self.reserve and 'Stop reserving its edge' or 'Reserve its edge'})
    self:set('visible', {text = self.visible and 'Hide it' or 'Show it again'})
end

function Banner:update(dt)
    Banner.super.update(self, dt)
    self:countGameTaps()

    self.insetsTime = self.insetsTime + haylen.unscaledDelta()
    if self.insetsTime >= Banner.insetsInterval then
        self.insetsTime = 0
        local reserved = viewport.reservedInsets()
        local safe = viewport.safeRect()
        local text = string.format('The banner reserves %.0f points at the top and %.0f at the bottom, so the UI lays out from y %.0f to %.0f.', reserved.top, reserved.bottom, safe.y, safe.y + safe.height)
        if text ~= self.safeText then
            self.safeText = text
            self:set('safeArea', {text = text})
        end
    end
end

-- Presses of the mouse and fingers that land on the tap zone reached the app through the overlay.
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
            self.ripples[#self.ripples + 1] = {x = x, y = y, at = haylen.elapsed()}
            self.results:set('game', 'pass', 'Other taps reach the app', string.format('The app received %d %s on the tap zone.', self.gameTaps, self.gameTaps == 1 and 'tap' or 'taps'))
        end
    end
end

-- The tap zone draws its own rounded card instead of the square stage, with the count of the taps and a ring that spreads from each one.
function Banner:render()
    if not self.area then
        return
    end
    graphics2d.beginWorld(self.camera)
    self:draw(self.area)
end

function Banner:draw(area)
    graphics2d.drawShape({0, 0, area.width, area.height}, {radius = 16, color = Test.surface, borderWidth = 2, borderColor = Test.line})
    local count = self.gameTaps == 0 and 'Tap or click anywhere here' or string.format('%d %s reached the app', self.gameTaps, self.gameTaps == 1 and 'tap' or 'taps')
    graphics2d.drawText(nil, count, area.width / 2, area.height / 2 - 8, {size = 34, color = Test.ink, anchor = {0.5, 1}, layer = 2})
    graphics2d.drawText(nil, 'The overlay of the banner lets every other tap through.', area.width / 2, area.height / 2 + 8, {size = 22, color = Test.muted, anchor = {0.5, 0}, maxWidth = area.width - 64, layer = 2})
    local now = haylen.elapsed()
    for index = #self.ripples, 1, -1 do
        local ripple = self.ripples[index]
        local age = (now - ripple.at) / Banner.rippleSeconds
        if age >= 1 then
            table.remove(self.ripples, index)
        else
            local alpha = math.floor((1 - age) * 255)
            graphics2d.drawRing(ripple.x, ripple.y, 16 + age * 70, 5, string.format('#%02XF2B23A', alpha), {layer = 3})
            graphics2d.drawCircle(ripple.x, ripple.y, 10 * (1 - age), string.format('#%02XF2B23A', alpha), {layer = 3})
        end
    end
end

return Banner
