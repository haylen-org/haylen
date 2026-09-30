-- Edge to edge: the app draws its world over the whole screen, under the notch, the rounded corners and the home indicator, while a screen document keeps its controls in the safe area with `ui.safeArea`.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local EdgeToEdge = haylen.class('EdgeToEdge', sample.Test)

EdgeToEdge.hints = 'Turn edge to edge off to see the world boxed into the safe area, which leaves the edges of the screen empty. The HUD stays in the safe area either way, and one label is anchored to the screen on purpose.'
EdgeToEdge.focus = 'edge'

function EdgeToEdge:init(entry)
    EdgeToEdge.super.init(self, entry)
    self.edge = true
    self.border = true
end

function EdgeToEdge:controls()
    return {
        ui.toggle{id = 'edge', text = 'Draw the world edge to edge', checked = self.edge, onChange = function(event) self.edge = event.checked end},
        ui.toggle{id = 'border', text = 'Show the border of the safe area', checked = self.border, onChange = function(event) self.border = event.checked end},
    }
end

-- The HUD is a screen document: `ui.safeArea` keeps its column inside the safe area, and the label outside it sits against the screen.
function EdgeToEdge:started()
    self.hud = sample.mount(self, ui.stack{
        ui.label{text = 'Anchored to the screen, under the notch', anchor = 'topLeft', anchorTo = 'screen', margin = 12, color = 'onAccent', outline = '#FF000000', outlineWidth = 3},
        ui.safeArea{
            ui.column{padding = 24,
                ui.row{gap = 16,
                    ui.circularProgress{id = 'health', value = 0.8, size = 72, tone = 'success', text = 'HP'},
                    ui.label{text = 'Day 3', font = 'heading', color = 'onAccent', outline = '#FF000000', outlineWidth = 3},
                    ui.spacer{grow = 1},
                    ui.badge{text = '12 coins', tone = 'warning', solid = true},
                },
                ui.spacer{grow = 1},
                ui.row{gap = 16, ui.spacer{grow = 1}, ui.button{text = 'Map', focusable = false}, ui.button{text = 'Bag', variant = 'primary', focusable = false}},
            },
        },
    }, {placement = 'screen'})
end

function EdgeToEdge:update(dt)
    local top, right, bottom, left = sample.insets()
    self:setStatus(string.format('insets top %.0f, right %.0f, bottom %.0f, left %.0f', top, right, bottom, left))
end

-- A sea at sunset with sailing boats, drawn over the visible screen or only over the safe area.
function EdgeToEdge:render()
    local area = self.edge and viewport.visibleRect() or viewport.safeRect()
    local time = haylen.elapsed()
    graphics2d.beginScreen()
    local horizon = area.y + area.height * 0.58
    graphics2d.drawMesh(nil, {
        {x = area.x, y = area.y, color = '#FF2B1E55'}, {x = area:right(), y = area.y, color = '#FF2B1E55'},
        {x = area:right(), y = horizon, color = '#FFF28E5B'}, {x = area.x, y = horizon, color = '#FFF28E5B'},
    }, {1, 2, 3, 1, 3, 4})
    graphics2d.drawCircle(area.x + area.width * 0.7, horizon - 60, 90, '#FFFFD27A')
    graphics2d.drawRect({area.x, horizon, area.width, area:bottom() - horizon}, '#FF3B3A7A')
    for row = 0, 7 do
        local y = horizon + 30 + row * 40
        local points = {}
        for step = 0, 40 do
            local x = area.x + area.width * step / 40
            points[#points + 1] = {x, y + math.sin(step * 0.8 + time * 2 + row) * 6}
        end
        graphics2d.drawPolyline(points, 3, '#60FFFFFF', false)
    end
    for index = 0, 2 do
        local x = area.x + (area.width + 400) * ((time * 0.04 + index / 3) % 1) - 200
        local y = horizon + 70 + index * 110
        graphics2d.drawPolygon({{x - 70, y}, {x + 70, y}, {x + 45, y + 30}, {x - 45, y + 30}}, '#FF4A2A14')
        graphics2d.drawPolygon({{x, y - 110}, {x, y - 6}, {x + 60, y - 6}}, '#FFF3E6C4')
    end
    if self.border then
        graphics2d.drawRectOutline(viewport.safeRect(), 3, '#FF3AA8E0', {layer = 1})
    end
end

return EdgeToEdge
