-- UI theme surfaces: a theme whose panels, banner, buttons, tracks, fills and knobs are nine-slice images. The gray images take the color of each component through `colorize`, so one fill image paints every tone. The theme applies to the whole screen while the test runs.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local UiTheme = haylen.class('UiTheme', sample.Test)

local kDemoWidth = 640
local kTones = {'accent', 'success', 'warning', 'danger'}
local kCode = [[
ui.setTheme(ui.addTheme({name = 'workshop', surfaces = {
    button = {image = 'ui/button.png', slice = 16, padding = {0, 0, 4, 0}, colorize = true},
    track = {image = 'ui/track.png', slice = 11, padding = 5},  trackFill = {image = 'ui/fill.png', slice = 7, colorize = true},
}}, 'light'))  -- One gray fill image becomes the accent, success, warning and danger bars.]]

-- The theme document, with `colorize` on or off for the surfaces that take the color of their component.
local function theme(colorize)
    local button = function(image, padding) return {image = image, slice = 16, padding = padding, colorize = colorize} end
    return {
        name = 'workshop',
        colors = {
            window = '#FFFBF3E1', panel = '#FFF2E3C6', raised = '#FFFBF3E1', text = '#FF3A2A1E', textMuted = '#FF6E5A45',
            accent = '#FF3E8BA3', accentHover = '#FF4F9DB5', accentStrong = '#FF2C6F86', accentText = '#FF2C6F86',
            danger = '#FFD9534A', success = '#FF6CBF4F', warning = '#FFF0A63C', border = '#FF6E5A45', borderStrong = '#FF3A2A1E', focus = '#FFF2C14E',
        },
        metrics = {controlHeight = 68, sliderTrackHeight = 24, sliderKnobSize = 40, progressHeight = 30, toggleWidth = 84, toggleHeight = 40},
        surfaces = {
            panel = {image = 'ui/panel.png', slice = 20, padding = 10},
            card = {image = 'ui/panel.png', slice = 20, padding = 10, tint = '#FFFFF8EC'},
            banner = {image = 'ui/banner.png', slice = {10, 40}, padding = {8, 44}, colorize = colorize},
            button = button('ui/button.png', {0, 0, 4, 0}),
            buttonPressed = button('ui/button_pressed.png', {4, 0, 0, 0}),
            buttonPrimary = button('ui/button.png', {0, 0, 4, 0}),
            buttonPrimaryPressed = button('ui/button_pressed.png', {4, 0, 0, 0}),
            buttonDestructive = button('ui/button.png', {0, 0, 4, 0}),
            buttonDestructivePressed = button('ui/button_pressed.png', {4, 0, 0, 0}),
            track = {image = 'ui/track.png', slice = 11, padding = 5},
            trackFill = {image = 'ui/fill.png', slice = 7, colorize = colorize},
            knob = {image = 'ui/knob.png'},
        },
    }
end

function UiTheme:enter()
    self.previousTheme = ui.theme()
    ui.setTheme(ui.addTheme(theme(true), 'light'))
    self:frame({
        hint = 'Turn "colorize" off to see the gray images as they are.',
        code = kCode,
        controls = {
            ui.toggle{id = 'colorize', text = 'Colorize', checked = true, onChange = function(event) ui.addTheme(theme(event.checked), 'light') end},
            ui.formField{label = 'Progress', ui.slider{id = 'progress', min = 0, max = 1, value = 0.6, showValue = true, onChange = function(event) self:fill(event.value) end}},
            ui.button{id = 'primary', text = 'Primary', variant = 'primary'},
        },
        focus = 'colorize',
    })

    local bars = {}
    for index, tone in ipairs(kTones) do
        bars[index] = ui.progress{id = tone, value = 0.6, tone = tone, text = tone}
    end
    self.demo = ui.mount(ui.column{id = 'demo', anchor = 'topLeft', width = kDemoWidth, gap = 16, onCancel = sample.back,
        ui.pageHeader{title = 'Workshop', caption = 'Every surface is a nine-slice', banner = true, textAlign = 'center'},
        ui.panel{gap = 14,
            ui.row{gap = 12,
                ui.button{id = 'default', text = 'Default', grow = 1},
                ui.button{id = 'accept', text = 'Primary', variant = 'primary', grow = 1},
                ui.button{id = 'delete', text = 'Destructive', variant = 'destructive', grow = 1},
            },
            ui.column{gap = 10, children = bars},
            ui.slider{id = 'volume', value = 0.5, showValue = true},
        },
    }, {owner = self, layer = 1})
end

function UiTheme:exit()
    ui.setTheme(self.previousTheme)
end

function UiTheme:fill(value)
    for _, tone in ipairs(kTones) do
        self.demo:set(tone, {value = value})
    end
end

-- Places the demo in the left part of the stage, which the safe area and the frame decide.
function UiTheme:resize(area)
    local stage, safe = self.document:bounds('stage'), viewport.safeRect()
    self.demo:set('demo', {margin = {stage.y - safe.y + 20, 0, 0, stage.x - safe.x + 20}})
end

function UiTheme:update(dt)
    UiTheme.super.update(self, dt)
    local fill = ui.themeSurface('trackFill')
    self:status(string.format('theme %s   trackFill borders %s   colorize %s', ui.theme(), table.concat(fill.slice.borders, ', '), fill.colorize))
end

-- The theme writes dark text, so the page behind the frame is light while the test runs.
function UiTheme:render()
    graphics2d.beginScreen({order = -1})
    graphics2d.drawRect(viewport.visibleRect(), '#FFE9DCC0')
    UiTheme.super.render(self)
end

-- Draws the fill image of the theme once per tone, multiplied by the color of the tone as `colorize` does.
function UiTheme:draw(area)
    local left = kDemoWidth + 70
    local fill, button = ui.themeSurface('trackFill'), ui.themeSurface('button')
    graphics2d.drawText(nil, 'ui/fill.png times each tone', left, 30, {size = 24, color = sample.ink})
    for index, tone in ipairs(kTones) do
        graphics2d.drawNineSlice(fill.slice, {left, 50 + index * 46, area.width - left - 40, 30}, ui.themeColor(tone))
    end
    graphics2d.drawText(nil, 'ui/button.png times raised, accent and danger', left, 300, {size = 24, color = sample.ink})
    for index, role in ipairs({'raised', 'accent', 'danger'}) do
        local width = (area.width - left - 60) / 3
        graphics2d.drawNineSlice(button.slice, {left + (index - 1) * (width + 10), 340, width, 64}, ui.themeColor(role))
    end
end

return UiTheme
