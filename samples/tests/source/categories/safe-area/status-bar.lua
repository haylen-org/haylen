-- The functions `window.setStatusBar` and `window.setStatusBarColor` show the status bar of a phone or tablet with light, dark or automatic content, and paint a color behind it, while the safe area leaves it out.
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local StatusBar = haylen.class('StatusBar', Test)

-- The background "none" has no color, so the engine paints nothing behind the status bar.
StatusBar.colors = {night = '#FF1E2A44', paper = '#FFF4EFE3', leaf = '#FF4C8B3F'}

StatusBar.platforms = [==[
[table=2][cell padding=6][b]Platform[/b][/cell][cell padding=6][b]What the status bar does[/b][/cell][cell padding=6]iPhone and iPad[/cell][cell padding=6]Shows through the view controller of the window, and an iPhone hides it in landscape.[/cell][cell padding=6]Android[/cell][cell padding=6]Shows over the app, and a hidden status bar shows for a while after a swipe from the top edge.[/cell][cell padding=6]Desktop, Mac Catalyst, TVs and browsers[/cell][cell padding=6]Have no status bar over the app, so nothing changes.[/cell][/table]]==]

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
        focus = 'bar',
        content = {ui.row{gap = 24, grow = 1, align = 'stretch',
            ui.card{width = 760, align = 'start', gap = 12,
                ui.sectionTitle{text = 'Status bar'},
                ui.segmentedControl{id = 'bar', items = {{id = 'hidden', text = 'Hidden'}, {id = 'auto', text = 'Auto'}, {id = 'light', text = 'Light'}, {id = 'dark', text = 'Dark'}}, selected = self.bar, onChange = function(event)
                    self.bar = event.value
                    window.setStatusBar(event.value)
                end},
                ui.sectionTitle{text = 'Background'},
                ui.segmentedControl{id = 'color', items = {{id = 'none', text = 'None'}, {id = 'night', text = 'Night'}, {id = 'paper', text = 'Paper'}, {id = 'leaf', text = 'Leaf'}}, selected = self.color, onChange = function(event)
                    self.color = event.value
                    window.setStatusBarColor(StatusBar.colors[event.value])
                end},
            },
            ui.card{grow = 1, align = 'start', gap = 12, ui.sectionTitle{text = 'Platforms'}, ui.richText{text = StatusBar.platforms}},
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
    self:status(string.format('Status bar "%s" with the background "%s", and the safe area starts %.0f units below the top on "%s".', window.statusBar(), self.color, top, haylen.platform))
end

return StatusBar
