-- Locking the orientation: `window.lockOrientation` keeps the screen in portrait or landscape, or lets it turn again with `any`, and each platform applies it its own way.
local haylen = require('haylen')
local ui = require('haylen.ui')
local window = require('haylen.window')

local sample = require('sample')

local Lock = haylen.class('Lock', sample.Test)

Lock.hints = 'The file "app.json" sets the orientations an app starts with, "any" in this sample, and leaving this test lets the screen turn freely again. Browsers usually lock only a fullscreen page on a phone, so turn fullscreen on first there.'
Lock.focus = 'lock'

local kPlatforms = [==[
[table=2][cell padding=6][b]Platform[/b][/cell][cell padding=6][b]What the lock does[/b][/cell][cell padding=6]Android[/cell][cell padding=6]Locks the activity with "setRequestedOrientation".[/cell][cell padding=6]iPhone and iPad[/cell][cell padding=6]Asks UIKit for the new orientations and turns the screen at once.[/cell][cell padding=6]Browsers[/cell][cell padding=6]The Screen Orientation API, which usually works only for a fullscreen page on a phone.[/cell][cell padding=6]Desktop, Mac Catalyst and TVs[/cell][cell padding=6]Ignore it, since their screens do not turn.[/cell][/table]]==]

function Lock:init(entry)
    Lock.super.init(self, entry)
    self.lock = 'any'
end

function Lock:content()
    return ui.row{gap = 24,
        sample.section('Lock', {width = 760, align = 'start',
            ui.segmentedControl{id = 'lock', items = {{id = 'portrait', text = 'Portrait'}, {id = 'landscape', text = 'Landscape'}, {id = 'any', text = 'Any'}}, selected = self.lock, onChange = function(event)
                self.lock = event.value
                window.lockOrientation(event.value)
            end},
            ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = window.fullscreen(), onChange = function(event)
                window.setFullscreen(event.checked)
            end},
        }),
        sample.section('Platforms', {grow = 1, align = 'start', ui.richText{text = kPlatforms}}),
    }
end

function Lock:exit()
    window.lockOrientation('any')
end

function Lock:update(dt)
    self:setStatus(string.format('Locked to "%s", the screen is "%s" on "%s"', self.lock, window.orientation(), haylen.platform))
end

return Lock
