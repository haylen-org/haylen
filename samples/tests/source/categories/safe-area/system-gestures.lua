-- The function `window.setSystemGestures` lets the swipes at the edges of a phone or tablet act at once, defers them to a second swipe so the first one reaches the app, or hides the home indicator until a touch, and the bands at the edges of the screen light up for every swipe that reached the app.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')
local areas = require('categories.safe-area.areas')

local SystemGestures = haylen.class('SystemGestures', Test)

SystemGestures.code = [[
window.setSystemGestures('deferred')  -- Or "system" and "autoHideIndicator", which "window.systemGestures" of "app.json" sets at start.
print(window.systemGestures())
for _, touch in ipairs(input.touches()) do
    local fromEdge = touch.phase == 'began' and touch.startX < visible.x + 80  -- The first swipe from a deferred edge reaches the app.
end]]

SystemGestures.platforms = {
    {'iPhone and iPad', 'The view controller of the window defers every edge, so the first swipe only wakes the home indicator, or hides the home indicator until a touch.'},
    {'Android', 'The navigation bar shows only for "system". Otherwise the bars hide, so the first swipe from the bottom or a side only shows them for a while, and deferred gestures keep the back gesture off the lowest 200 dp of each side edge, the most Android allows.'},
    {'Desktop, Mac Catalyst, TVs, Apple Vision Pro and browsers', 'Have no such gestures that an app can defer, so nothing changes.'},
}

SystemGestures.modes = {{id = 'system', text = 'System'}, {id = 'deferred', text = 'Deferred'}, {id = 'autoHideIndicator', text = 'Hide indicator'}}
SystemGestures.states = {system = 'act at once', deferred = 'wait for a second swipe', autoHideIndicator = 'act at once, with the home indicator hidden until a touch'}
SystemGestures.edges = {{id = 'top', text = 'Top'}, {id = 'bottom', text = 'Bottom'}, {id = 'left', text = 'Left'}, {id = 'right', text = 'Right'}}

-- How far from an edge of the visible screen a finger lands for its swipe to count as one from that edge, in design units, and how long the band of the edge stays lit.
SystemGestures.reach = 80
SystemGestures.glow = 0.8

function SystemGestures:init(entry)
    SystemGestures.super.init(self, entry)
    self.initial = window.systemGestures()
    self.swipes = {top = 0, bottom = 0, left = 0, right = 0}
    self.lit = {top = 0, bottom = 0, left = 0, right = 0}
end

function SystemGestures:enter()
    local counts = {}
    for index, edge in ipairs(SystemGestures.edges) do
        counts[index] = ui.row{gap = 24, ui.label{text = edge.text, grow = 1}, ui.label{id = 'swipes-' .. edge.id, text = '0'}}
    end
    self:frame{
        hint = 'The file "app.json" leaves the edges of this project deferred, and leaving this test brings that back. Swipe in from an edge of the screen with each choice and watch which swipes reach the app.',
        code = SystemGestures.code,
        focus = 'gestures',
        fixed = true,
        content = {ui.row{gap = 24, grow = 1, align = 'stretch',
            ui.scroll{grow = 1, align = 'stretch', ui.column{gap = 24, align = 'stretch',
                ui.card{gap = 12,
                    ui.sectionTitle{text = 'System gestures'},
                    ui.segmentedControl{id = 'gestures', items = SystemGestures.modes, selected = window.systemGestures(), onChange = function(event) window.setSystemGestures(event.value) end},
                },
                ui.card{gap = 12, ui.sectionTitle{text = 'Platforms'}, areas.platformTable('What the gestures do', SystemGestures.platforms)},
            }},
            ui.card{width = 520, gap = 12, align = 'stretch', ui.sectionTitle{text = 'Swipes that reached the app'}, ui.column{gap = 12, align = 'stretch', children = counts}},
        }},
    }
end

function SystemGestures:exit()
    window.setSystemGestures(self.initial)
    SystemGestures.super.exit(self)
end

-- The edge of the visible screen nearest to where a finger landed, when it landed close enough to one.
function SystemGestures:edgeOf(x, y)
    local visible = viewport.visibleRect()
    local distances = {left = x - visible.x, right = visible:right() - x, top = y - visible.y, bottom = visible:bottom() - y}
    local nearest, distance = nil, SystemGestures.reach
    for edge, value in pairs(distances) do
        if value < distance then
            nearest, distance = edge, value
        end
    end
    return nearest
end

function SystemGestures:update(dt)
    SystemGestures.super.update(self, dt)
    for edge, time in pairs(self.lit) do
        self.lit[edge] = math.max(0, time - dt)
    end
    for _, touch in ipairs(input.touches()) do
        local edge = touch.phase == 'began' and self:edgeOf(touch.startX, touch.startY)
        if edge then
            self.swipes[edge] = self.swipes[edge] + 1
            self.lit[edge] = SystemGestures.glow
            self:set('swipes-' .. edge, {text = tostring(self.swipes[edge])})
        end
    end
    local total = self.swipes.top + self.swipes.bottom + self.swipes.left + self.swipes.right
    self:status(string.format('The system gestures %s, and %d %s from an edge reached the app.', SystemGestures.states[window.systemGestures()], total, total == 1 and 'swipe' or 'swipes'))
end

-- The bands along the edges of the screen, where the system takes its swipes, light up over the frame when a swipe from them reached the app.
function SystemGestures:render()
    SystemGestures.super.render(self)
    local visible = viewport.visibleRect()
    local reach = SystemGestures.reach
    local bands = {
        top = {visible.x, visible.y, visible.width, reach},
        bottom = {visible.x, visible:bottom() - reach, visible.width, reach},
        left = {visible.x, visible.y, reach, visible.height},
        right = {visible:right() - reach, visible.y, reach, visible.height},
    }
    graphics2d.beginScreen({order = 1})
    for edge, band in pairs(bands) do
        local glow = self.lit[edge] / SystemGestures.glow
        if glow > 0 then
            graphics2d.drawRect(band, {0.56, 0.69, 1, 0.5 * glow})
        end
    end
end

return SystemGestures
