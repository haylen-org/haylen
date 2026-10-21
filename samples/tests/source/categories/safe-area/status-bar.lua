-- The functions `window.setStatusBar` and `window.setStatusBarColor` show the status bar of a phone or tablet with light, dark or automatic content, and paint a color behind it, while the safe area leaves it out.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local StatusBar = haylen.class('StatusBar', Test)

StatusBar.code = [[
window.setStatusBar('light')  -- Or `hidden`, `auto` and `dark`, which `window.statusBar` of `app.json` sets at start.
window.setStatusBarColor('#FF1E2A44')  -- Painted behind it, since the app draws under it.
print(window.statusBar(), viewport.safeRect().y)  -- The safe area starts below it.]]

-- The background "none" has no color, so the engine paints nothing behind the status bar.
StatusBar.colors = {night = '#FF1E2A44', paper = '#FFF4EFE3', leaf = '#FF4C8B3F'}

StatusBar.platforms = '[table=2][cell bg=#33FFFFFF padding=10][b]Platform[/b][/cell][cell bg=#33FFFFFF padding=10][b]What the status bar does[/b][/cell]'
    .. '[cell padding=10]iPhone and iPad[/cell][cell padding=10]Shows through the view controller of the window, and an iPhone hides it in landscape.[/cell]'
    .. '[cell bg=#14FFFFFF padding=10]Android[/cell][cell bg=#14FFFFFF padding=10]Shows over the app, and a hidden status bar shows for a while after a swipe from the top edge.[/cell]'
    .. '[cell padding=10]Desktop, Mac Catalyst, TVs and browsers[/cell][cell padding=10]Have no status bar over the app, so nothing changes.[/cell][/table]'
-- The ink of the content of the bar for each choice, where automatic content follows the theme of the UI.
StatusBar.inks = {light = '#FFFFFFFF', dark = '#FF15172A'}
StatusBar.names = {none = 'no color', night = 'the night color', paper = 'the paper color', leaf = 'the leaf color'}

function StatusBar:init(entry)
    StatusBar.super.init(self, entry)
    self.initialBar = window.statusBar()
    self.initialColor = window.statusBarColor()
    self.bar = self.initialBar
    self.color = 'none'
end

function StatusBar:enter()
    self:frame{
        hint = 'The file "app.json" hides the status bar of this project, and leaving this test hides it again. Light content suits a dark background and dark content a light one, while automatic content follows the theme of the system.',
        code = StatusBar.code,
        focus = 'bar',
        fixed = true,
        content = {ui.row{gap = 24, grow = 1, align = 'stretch',
            ui.scroll{grow = 1, align = 'stretch', ui.column{gap = 24, align = 'stretch',
                ui.card{gap = 12,
                    ui.sectionTitle{text = 'Status bar'},
                    ui.segmentedControl{id = 'bar', items = {{id = 'hidden', text = 'Hidden'}, {id = 'auto', text = 'Auto'}, {id = 'light', text = 'Light'}, {id = 'dark', text = 'Dark'}}, selected = self.bar, onChange = function(event)
                        self.bar = event.value
                        window.setStatusBar(event.value)
                    end},
                    ui.sectionTitle{text = 'Background', margin = {12, 0, 0, 0}},
                    ui.segmentedControl{id = 'color', items = {{id = 'none', text = 'None'}, {id = 'night', text = 'Night'}, {id = 'paper', text = 'Paper'}, {id = 'leaf', text = 'Leaf'}}, selected = self.color, onChange = function(event)
                        self.color = event.value
                        window.setStatusBarColor(StatusBar.colors[event.value])
                    end},
                },
                ui.card{gap = 12, ui.sectionTitle{text = 'Platforms'}, ui.richText{text = StatusBar.platforms}},
            }},
            ui.card{width = 520, gap = 12, align = 'stretch', ui.sectionTitle{text = 'On a phone'}, ui.spacer{id = 'preview', grow = 1}},
        }},
    }
end

function StatusBar:exit()
    window.setStatusBar(self.initialBar)
    window.setStatusBarColor(self.initialColor)
    StatusBar.super.exit(self)
end

function StatusBar:update(dt)
    StatusBar.super.update(self, dt)
    local top = areas.insets()
    self:status(string.format('The status bar is %s on %s, and the safe area starts %s units below the top of this screen.', window.statusBar(), StatusBar.names[self.color], areas.number(top)))
end

-- The phone of the preview shows its status bar the way the choices ask: hidden, or with the time, the signal and the battery in light or dark ink over the background color.
function StatusBar:render()
    local bounds = self.gui and self.gui:bounds('preview')
    if not bounds then
        return
    end
    -- The phone draws over the card that holds its room.
    graphics2d.beginScreen({order = 1})
    local height = math.min(bounds.height * 0.94, bounds.width * 1.6)
    local width = height * 0.49
    local x, y = bounds.x + bounds.width / 2, bounds.y + bounds.height / 2
    Device.draw{kind = 'island', x = x, y = y, width = width, height = height, screen = '#FF2F86F6'}
    if self.bar == 'hidden' then
        return
    end
    local screenWidth, screenHeight = Device.screenSize('island', width, height)
    local left, top = x - screenWidth / 2, y - screenHeight / 2
    local barHeight = width * 0.15
    local color = StatusBar.colors[self.color]
    if color then
        graphics2d.pushMask(function()
            graphics2d.drawShape({left, top, screenWidth, screenHeight}, {radius = width * 0.15 - width * 0.045, color = '#FFFFFFFF'})
        end, {layer = 3})
        graphics2d.drawRect({left, top, screenWidth, barHeight}, color)
        graphics2d.popMask()
    end
    local ink = StatusBar.inks[self.bar] or (ui.theme() == 'light' and StatusBar.inks.dark or StatusBar.inks.light)
    local middle = top + barHeight * 0.52
    Test.caption('9:41', left + screenWidth * 0.17, middle, {size = width * 0.07, color = ink, anchor = {0.5, 0.5}, layer = 6})
    for index = 0, 3 do
        local barX = left + screenWidth * 0.68 + index * width * 0.022
        local tall = width * (0.016 + index * 0.009)
        graphics2d.drawShape({barX, middle + width * 0.025 - tall, width * 0.014, tall}, {radius = width * 0.004, color = ink, layer = 6})
    end
    local batteryX = left + screenWidth * 0.82
    graphics2d.drawShape({batteryX, middle - width * 0.018, width * 0.075, width * 0.036}, {radius = width * 0.01, color = '#00000000', borderWidth = math.max(1.5, width * 0.005), borderColor = ink, layer = 6})
    graphics2d.drawShape({batteryX + width * 0.008, middle - width * 0.01, width * 0.045, width * 0.02}, {radius = width * 0.005, color = ink, layer = 6})
end

return StatusBar
