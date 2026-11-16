-- Glass: `graphics2d.drawGlass` over the moving landscape, a bar across the top, a round lens at the cursor and a card, each blurred, richer, tinted, lit along its rim and bent at its edge, with every value on the controls. The interface takes the same glass with `glass = true` on a panel, a card and a dialog, which draw as plain surfaces while the system asks for reduced transparency.
local assets = require('haylen.assets')
local debug = require('haylen.debug')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')

local ParticleTest = require('categories.particles.particle-test')
local Scenery = require('categories.effects.scenery')

local Glass = haylen.class('Glass', ParticleTest)

-- A dialog of glass, presented over the landscape that goes on behind it.
local Paused = haylen.class('Paused', ui.Screen)

function Paused:build()
    return ui.dialog{glass = true, title = 'Paused', message = 'The landscape goes on behind the glass.', buttons = {{id = 'resume', text = 'Resume'}},
        onAnswer = function() self.presentation:dismiss() end}
end

Glass.code = [[
graphics2d.drawGlass({-800, -480, 1600, 140}, {radius = 70, blur = 24, saturation = 1.6,
    tint = '#33FFFFFF', rim = 0.6, refraction = 12})
graphics2d.drawGlass({x - 110, y - 110, 220, 220}, {kind = 'ellipse', blur = 4, refraction = 30, rim = 0.8})
ui.panel{glass = true, ui.label{text = 'Glass of the theme'}}
ui.dialog{glass = true, title = 'Paused'}  -- Plain with reduced transparency.]]

Glass.sliders = {
    {'blur', 'Blur radius', 0, 96, 0},
    {'saturation', 'Saturation', 0, 2.5, 2},
    {'brightness', 'Brightness', 0.4, 1.6, 2},
    {'tint', 'White haze', 0, 0.8, 2},
    {'rim', 'Rim of light', 0, 1, 2},
    {'refraction', 'Refraction', 0, 40, 0},
}

function Glass:init(entry)
    Glass.super.init(self, entry)
    self.scenery = Scenery()
    self.knight = assets.texture('sprites/images/hero.png', {filter = 'linear'})
    self.look = {blur = 24, saturation = 1.6, brightness = 1.05, tint = 0.2, rim = 0.6, refraction = 12}
    self.options = {}
    self.interfaceGlass = true
    self.drawn = true
end

function Glass:enter()
    local controls = {
        ui.toggle{id = 'drawn', text = 'Glass shapes on the landscape', checked = self.drawn, onChange = function(event) self.drawn = event.checked end},
        ui.toggle{id = 'interface', text = 'Glass panel and card of the interface', checked = self.interfaceGlass, onChange = function(event)
            self.interfaceGlass = event.checked
            self.overlay:set('ui-panel', {visible = event.checked})
            self.overlay:set('ui-card', {visible = event.checked})
        end},
        ui.button{id = 'dialog', text = 'Open a dialog of glass', onClick = function() ui.present(Paused(), {style = 'dialog', owner = self}) end},
    }
    for _, slider in ipairs(Glass.sliders) do
        local key = slider[1]
        controls[#controls + 1] = ui.formField{label = slider[2], ui.slider{id = key, min = slider[3], max = slider[4], value = self.look[key], showValue = true, decimals = slider[5], onChange = function(event)
            self.look[key] = event.value
        end}}
    end
    self:frame{
        hint = 'The lens follows the cursor. The bar and the card stay in place while the landscape moves behind them.',
        code = Glass.code,
        cursor = true,
        controls = controls,
    }
    self.overlay = ui.mount(ui.column{id = 'glass-panel', anchor = 'bottomLeft', margin = 24, gap = 16, width = 460,
        ui.panel{id = 'ui-panel', glass = true, gap = 8, ui.label{text = 'Glass panel', font = 'heading'}, ui.label{text = 'The theme gives it its blur, tint, rim and bend.', color = 'textMuted'}},
        ui.card{id = 'ui-card', glass = true, gap = 8, ui.label{text = 'Glass card', font = 'heading'}, ui.label{text = 'Reduced transparency draws it plain.', color = 'textMuted'}},
    }, {owner = self, layer = 1})
    self:keepInRegion(self.overlay, 'glass-panel', 24)
end

function Glass:update(dt)
    Glass.super.update(self, dt)
    self.scenery:update(dt)
    local look = self.look
    local options = self.options
    options.blur, options.saturation, options.brightness = look.blur, look.saturation, look.brightness
    options.tint = {1, 1, 1, look.tint}
    options.rim, options.refraction = look.rim, look.refraction
    -- The time of the GPU shows what the glass costs next to the frame without it, where the graphics API measures it.
    local stats = graphics2d.stats()
    local gpu = debug.frame().gpuMilliseconds
    local timing = gpu and string.format(' The GPU took %.2f ms for the last frame.', gpu) or ''
    self:report('The glass blurs over %.0f units with a refraction of %.0f, and the frame takes %d passes and %d draw calls.%s', look.blur, look.refraction, stats.passes, stats.drawCalls, timing)
end

function Glass:draw(area)
    self.scenery:draw()
    local time = self.scenery.time
    graphics2d.draw(self.knight, 120, Scenery.ground, {scaleX = 1.3, scaleY = 1.3, pivotY = 1, layer = 1})
    for index = 0, 5 do
        graphics2d.drawCircle(-750 + index * 300, -200 + math.sin(time * 1.3 + index) * 160, 60, index % 2 == 0 and '#FFF0473E' or '#FFFFC93C', {layer = 2})
    end

    if not self.drawn then
        return
    end
    local options = self.options
    options.kind, options.radius, options.layer = 'rectangle', 70, 10
    graphics2d.drawGlass({-800, -480, 1600, 140}, options)
    options.radius = 40
    graphics2d.drawGlass({420, -60, 420, 300}, options)
    options.kind, options.layer = 'ellipse', 11
    graphics2d.drawGlass({self.cursorX - 110, self.cursorY - 110, 220, 220}, options)
end

return Glass
