-- The function `window.lockOrientation` keeps the screen in portrait or landscape, or lets it turn again with `any`, and each platform applies it its own way.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local tween = require('haylen.tween')
local ui = require('haylen.ui')
local window = require('haylen.window')

local Device = require('categories.safe-area.device')
local Test = require('harness.test')

local Lock = haylen.class('Lock', Test)

Lock.code = [[
window.lockOrientation('portrait')  -- Or `landscape` and `any`, which lets the screen turn.
window.setFullscreen(true)  -- Browsers usually lock only a fullscreen page.
print(window.orientation())]]

Lock.platforms = '[table=2][cell bg=#33FFFFFF padding=10][b]Platform[/b][/cell][cell bg=#33FFFFFF padding=10][b]What the lock does[/b][/cell]'
    .. '[cell padding=10]Android[/cell][cell padding=10]Locks the activity with "setRequestedOrientation".[/cell]'
    .. '[cell bg=#14FFFFFF padding=10]iPhone and iPad[/cell][cell bg=#14FFFFFF padding=10]Asks UIKit for the new orientations and turns the screen at once.[/cell]'
    .. '[cell padding=10]Browsers[/cell][cell padding=10]The Screen Orientation API, which usually works only for a fullscreen page on a phone.[/cell]'
    .. '[cell bg=#14FFFFFF padding=10]Desktop, Mac Catalyst and TVs[/cell][cell bg=#14FFFFFF padding=10]Ignore it, since their screens do not turn.[/cell][/table]'
-- The angle the phone of the preview turns to for each lock, where "any" leans it between the two.
Lock.angles = {portrait = 0, landscape = -math.pi / 2, any = -math.pi / 4}

function Lock:init(entry)
    Lock.super.init(self, entry)
    self.lock = 'landscape'
    self.fullscreen = window.fullscreen()
    self.phone = {angle = Lock.angles.landscape}
end

function Lock:enter()
    self:frame{
        hint = 'The file "app.json" sets the orientation an app starts with, "landscape" in this project, and leaving this test locks the screen to landscape again. Browsers usually lock only a fullscreen page on a phone, so turn fullscreen on first there.',
        code = Lock.code,
        focus = 'lock',
        content = {ui.row{gap = 24, grow = 1, align = 'stretch',
            ui.card{width = 760, align = 'stretch', gap = 12,
                ui.sectionTitle{text = 'Lock'},
                ui.segmentedControl{id = 'lock', items = {{id = 'portrait', text = 'Portrait'}, {id = 'landscape', text = 'Landscape'}, {id = 'any', text = 'Any'}}, selected = self.lock, onChange = function(event)
                    self.lock = event.value
                    window.lockOrientation(event.value)
                    tween.to(self.phone, 0.45, {angle = Lock.angles[event.value]}, {ease = 'backOut', owner = self, overwrite = true})
                end},
                ui.toggle{id = 'fullscreen', text = 'Fullscreen', checked = self.fullscreen, onChange = function(event)
                    window.setFullscreen(event.checked)
                end},
                ui.spacer{id = 'preview', grow = 1, minHeight = 260},
            },
            ui.card{grow = 1, align = 'start', gap = 12, ui.sectionTitle{text = 'Platforms'}, ui.richText{text = Lock.platforms}},
        }},
        fixed = true,
    }
end

function Lock:exit()
    window.lockOrientation('landscape')
    window.setFullscreen(self.fullscreen)
    Lock.super.exit(self)
end

function Lock:update(dt)
    Lock.super.update(self, dt)
    self:status(string.format('The lock is "%s" and the screen shows in %s.', self.lock, window.orientation()))
end

-- The preview holds a phone turned the way the lock keeps it, with a padlock that closes for portrait and landscape and opens for any.
function Lock:render()
    local bounds = self.gui and self.gui:bounds('preview')
    if not bounds then
        return
    end
    -- The preview draws over the card that holds its room.
    graphics2d.beginScreen({order = 1})
    local height = math.min(bounds.height * 0.95, bounds.width * 0.5)
    local x, y = bounds.x + bounds.width * 0.42, bounds.y + bounds.height / 2
    Device.draw{kind = 'island', x = x, y = y, width = height * 0.49, height = height, angle = self.phone.angle, screen = '#FF2F86F6'}
    self:drawPadlock(bounds.x + bounds.width * 0.78, y, height * 0.28, self.lock == 'any')
end

-- A padlock of the art style: a gold body with its keyhole under a steel shackle that lifts when it opens.
function Lock:drawPadlock(x, y, size, open)
    local outline = '#FF1E1B33'
    local lift = open and size * 0.3 or 0
    local shackle = size * 0.36
    graphics2d.drawArc(x, y - size * 0.1 - lift, shackle, size * 0.12, math.pi, math.pi * 2, outline, {layer = 1})
    graphics2d.drawArc(x, y - size * 0.1 - lift, shackle, size * 0.06, math.pi, math.pi * 2, '#FFD5D9E6', {layer = 2})
    graphics2d.drawShape({x - shackle - size * 0.06, y - size * 0.1 - lift, size * 0.12, size * 0.2 + lift}, {color = '#FFD5D9E6', borderWidth = size * 0.03, borderColor = outline, layer = 1})
    if not open then
        graphics2d.drawShape({x + shackle - size * 0.06, y - size * 0.1, size * 0.12, size * 0.2}, {color = '#FFD5D9E6', borderWidth = size * 0.03, borderColor = outline, layer = 1})
    end
    graphics2d.drawShape({x - size * 0.55, y, size * 1.1, size * 0.85}, {radius = size * 0.16, color = '#FFFFC93C', borderWidth = size * 0.06, borderColor = outline, layer = 3})
    graphics2d.drawShape({x - size * 0.45, y + size * 0.08, size * 0.5, size * 0.12}, {radius = size * 0.06, color = '#80FFFFFF', layer = 4})
    graphics2d.drawCircle(x, y + size * 0.4, size * 0.1, outline, {layer = 4})
    graphics2d.drawShape({x - size * 0.04, y + size * 0.42, size * 0.08, size * 0.22}, {radius = size * 0.04, color = outline, layer = 4})
end

return Lock
