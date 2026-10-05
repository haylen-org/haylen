-- The function `window.lockOrientation` keeps the screen in portrait or landscape, or lets it turn again with `any`, and each platform applies it its own way.
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Test = require('harness.test')

local Lock = haylen.class('Lock', Test)

Lock.platforms = [==[
[table=2][cell padding=6][b]Platform[/b][/cell][cell padding=6][b]What the lock does[/b][/cell][cell padding=6]Android[/cell][cell padding=6]Locks the activity with "setRequestedOrientation".[/cell][cell padding=6]iPhone and iPad[/cell][cell padding=6]Asks UIKit for the new orientations and turns the screen at once.[/cell][cell padding=6]Browsers[/cell][cell padding=6]The Screen Orientation API, which usually works only for a fullscreen page on a phone.[/cell][cell padding=6]Desktop, Mac Catalyst and TVs[/cell][cell padding=6]Ignore it, since their screens do not turn.[/cell][/table]]==]

function Lock:init(entry)
    Lock.super.init(self, entry)
    self.lock = 'landscape'
    self.fullscreen = window.fullscreen()
end

function Lock:enter()
    self:frame{
        hint = 'The file "app.json" sets the orientation an app starts with, "landscape" in this project, and leaving this test locks the screen to landscape again. Browsers usually lock only a fullscreen page on a phone, so turn fullscreen on first there.',
        focus = 'lock',
        content = {ui.row{gap = 24, grow = 1, align = 'stretch',
            ui.card{width = 760, align = 'start', gap = 12,
                ui.sectionTitle{text = 'Lock'},
                ui.segmentedControl{id = 'lock', items = {{id = 'portrait', text = 'Portrait'}, {id = 'landscape', text = 'Landscape'}, {id = 'any', text = 'Any'}}, selected = self.lock, onChange = function(event)
                    self.lock = event.value
                    window.lockOrientation(event.value)
                end},
                ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = self.fullscreen, onChange = function(event)
                    window.setFullscreen(event.checked)
                end},
            },
            ui.card{grow = 1, align = 'start', gap = 12, ui.sectionTitle{text = 'Platforms'}, ui.richText{text = Lock.platforms}},
        }},
    }
end

function Lock:exit()
    window.lockOrientation('landscape')
    window.setFullscreen(self.fullscreen)
    Lock.super.exit(self)
end

function Lock:update(dt)
    Lock.super.update(self, dt)
    self:status(string.format('Locked to "%s", the screen is "%s" on "%s".', self.lock, window.orientation(), haylen.platform))
end

return Lock
