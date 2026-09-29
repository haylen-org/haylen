-- Adaptive layout: a character screen that stacks its parts when the screen is taller than wide and places them side by side otherwise, rebuilt with document:replace when the shape changes.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')

local sample = require('sample')

local Adaptive = haylen.class('Adaptive', sample.Test)

Adaptive.hints = 'Turn a phone or a tablet, or make a desktop window taller than wide: the expand policy shows more design units along the long side, and the screen lays itself out again.'
Adaptive.focus = 'equip'

function Adaptive:init(entry)
    Adaptive.super.init(self, entry)
    self.last = ''
end

-- The parts of the screen, the same in both layouts.
function Adaptive:portrait()
    return ui.card{gap = 16, align = 'stretch',
        ui.row{gap = 24, ui.avatar{name = 'Ana Souza', size = 140}, ui.column{grow = 1, gap = 4, ui.label{text = 'Ana Souza', font = 'title'}, ui.label{text = 'Captain of the Gull, level 12', color = 'textMuted'}}},
        ui.row{gap = 12, ui.badge{text = 'Navigator', tone = 'accent', solid = true}, ui.badge{text = 'Fearless', tone = 'success'}, ui.badge{text = 'Seasick', tone = 'warning'}},
    }
end

function Adaptive:stats()
    return ui.card{gap = 12, align = 'stretch',
        ui.sectionTitle{text = 'Stats'},
        ui.progress{value = 0.82, tone = 'success', text = 'Health 82'},
        ui.progress{value = 0.45, tone = 'information', text = 'Stamina 45'},
        ui.progress{value = 0.6, text = 'Experience 60%'},
    }
end

function Adaptive:actions(stacked)
    local press = function(event) self.last = ', pressed ' .. event.id end
    local buttons = {
        ui.button{id = 'equip', text = 'Equip', variant = 'primary', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
        ui.button{id = 'train', text = 'Train', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
        ui.button{id = 'rest', text = 'Rest', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
    }
    return stacked and ui.column{gap = 12, children = buttons} or ui.row{gap = 12, children = buttons}
end

-- Portrait stacks everything in one column, and landscape puts the portrait on the left and the stats and actions on the right.
function Adaptive:layout(shape)
    if shape == 'portrait' then
        return ui.column{gap = 24, self:portrait(), self:stats(), self:actions(true)}
    end
    return ui.row{gap = 24,
        ui.column{grow = 1, gap = 24, align = 'start', self:portrait()},
        ui.column{grow = 1, gap = 24, align = 'start', self:stats(), self:actions(false)},
    }
end

function Adaptive:content()
    self.shape = sample.tall() and 'portrait' or 'landscape'
    return ui.column{id = 'layout', self:layout(self.shape)}
end

function Adaptive:update(dt)
    local shape = sample.tall() and 'portrait' or 'landscape'
    if shape ~= self.shape then
        self.shape = shape
        self.document:replace('layout', {self:layout(shape)})
        self.document:command('equip', 'focus')
    end
    local visible = viewport.visibleRect()
    self:setStatus(string.format('laid out for %s, visible %.0f x %.0f design units%s', shape, visible.width, visible.height, self.last))
end

return Adaptive
