-- UI theme surfaces: a theme whose panels, banner, buttons, tracks, fills and knobs are nine-slice images. The gray images take the color of each component through `colorize`, so one fill image paints every tone. The theme applies to the whole screen while the test runs.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local Test = require('harness.test')

local ThemeSurfaces = haylen.class('ThemeSurfaces', Test)

ThemeSurfaces.demoWidth = 640
ThemeSurfaces.tones = {'accent', 'success', 'warning', 'danger'}
ThemeSurfaces.code = [[
ui.setTheme(ui.addTheme({name = 'workshop', surfaces = {
    button = {image = 'nine-slice/theme/button.png', slice = 16, padding = {0, 0, 4, 0}, colorize = true},
    track = {image = 'nine-slice/theme/track.png', slice = 11, padding = 5},  trackFill = {image = 'nine-slice/theme/fill.png', slice = 7, colorize = true},
}}, 'light'))  -- One gray fill image becomes the accent, success, warning and danger bars.]]

-- The theme document, with `colorize` on or off for the surfaces that take the color of their component.
function ThemeSurfaces.theme(colorize)
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
            panel = {image = 'nine-slice/theme/panel.png', slice = 20, padding = 10},
            card = {image = 'nine-slice/theme/panel.png', slice = 20, padding = 10, tint = '#FFFFF8EC'},
            banner = {image = 'nine-slice/theme/banner.png', slice = {10, 40}, padding = {8, 44}, colorize = colorize},
            button = button('nine-slice/theme/button.png', {0, 0, 4, 0}),
            buttonPressed = button('nine-slice/theme/button_pressed.png', {4, 0, 0, 0}),
            buttonPrimary = button('nine-slice/theme/button.png', {0, 0, 4, 0}),
            buttonPrimaryPressed = button('nine-slice/theme/button_pressed.png', {4, 0, 0, 0}),
            buttonDestructive = button('nine-slice/theme/button.png', {0, 0, 4, 0}),
            buttonDestructivePressed = button('nine-slice/theme/button_pressed.png', {4, 0, 0, 0}),
            track = {image = 'nine-slice/theme/track.png', slice = 11, padding = 5},
            trackFill = {image = 'nine-slice/theme/fill.png', slice = 7, colorize = colorize},
            knob = {image = 'nine-slice/theme/knob.png'},
        },
    }
end

function ThemeSurfaces:enter()
    self.previousTheme = ui.theme()
    ui.setTheme(ui.addTheme(ThemeSurfaces.theme(true), 'light'))
    self:frame{
        code = ThemeSurfaces.code,
        hint = 'Turn "colorize" off to see the gray images as they are.',
        controls = {
            ui.toggle{id = 'colorize', text = 'Colorize', checked = true, onChange = function(event) ui.addTheme(ThemeSurfaces.theme(event.checked), 'light') end},
            ui.formField{label = 'Progress', ui.slider{id = 'progress', min = 0, max = 1, value = 0.6, showValue = true, onChange = function(event) self:fill(event.value) end}},
            ui.button{id = 'primary', text = 'Primary', variant = 'primary'},
        },
        focus = 'colorize',
    }

    local bars = {}
    for index, tone in ipairs(ThemeSurfaces.tones) do
        bars[index] = ui.progress{id = tone, value = 0.6, tone = tone, text = 'Tone "' .. tone .. '"'}
    end
    self.demo = ui.mount(ui.column{id = 'demo', anchor = 'topLeft', width = ThemeSurfaces.demoWidth, gap = 16, onCancel = function() self:cancel() end,
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

function ThemeSurfaces:exit()
    ui.setTheme(self.previousTheme)
    ThemeSurfaces.super.exit(self)
end

function ThemeSurfaces:fill(value)
    for _, tone in ipairs(ThemeSurfaces.tones) do
        self.demo:set(tone, {value = value})
    end
end

-- Places the demo in the left part of the play area, which the safe area and the frame decide.
function ThemeSurfaces:resize(area)
    local safe = viewport.safeRect()
    self.demo:set('demo', {margin = {self.stage.y - safe.y + 20, 0, 0, self.stage.x - safe.x + 20}})
end

function ThemeSurfaces:update(dt)
    ThemeSurfaces.super.update(self, dt)
    local fill = ui.themeSurface('trackFill')
    self:status(string.format('Theme "%s"   Borders of "trackFill" %s   Property "colorize" is "%s"', ui.theme(), table.concat(fill.slice.borders, ', '), fill.colorize))
end

-- The theme writes dark text, so the page behind the frame is light while the test runs.
function ThemeSurfaces:render()
    graphics2d.beginScreen({order = -1})
    graphics2d.drawRect(viewport.visibleRect(), '#FFE9DCC0')
    ThemeSurfaces.super.render(self)
end

-- Draws the fill image of the theme once per tone, multiplied by the color of the tone as `colorize` does.
function ThemeSurfaces:draw(area)
    local left = ThemeSurfaces.demoWidth + 70
    local fill, button = ui.themeSurface('trackFill'), ui.themeSurface('button')
    local width = area.width - left - 40
    Test.caption('The image "theme/fill.png" times each tone', left, 30, {size = 24, color = Test.ink, maxWidth = width})
    for index, tone in ipairs(ThemeSurfaces.tones) do
        graphics2d.drawNineSlice(fill.slice, {left, 50 + index * 46, width, 30}, ui.themeColor(tone))
    end
    Test.caption('The image "theme/button.png" times "raised", "accent" and "danger"', left, 300, {size = 24, color = Test.ink, maxWidth = width})
    local third = (width - 20) / 3
    for index, role in ipairs({'raised', 'accent', 'danger'}) do
        graphics2d.drawNineSlice(button.slice, {left + (index - 1) * (third + 10), 370, third, 64}, ui.themeColor(role))
    end
end

return ThemeSurfaces
