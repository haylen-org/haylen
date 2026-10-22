-- A character screen that stacks its parts when the screen is taller than wide and places them side by side otherwise, filling the region, rebuilt with `gui:replaceChildren` when the shape changes. The project starts locked to landscape, so the test lets the screen turn while it shows.
local haylen = require('haylen')
local ui = require('haylen.ui')
local viewport = require('haylen.viewport')
local window = require('haylen.window')

local Test = require('harness.test')

local Adaptive = haylen.class('Adaptive', Test)

Adaptive.code = [[
local visible = viewport.visibleRect()
local shape = visible.height > visible.width and 'portrait' or 'landscape'
if shape ~= self.shape then
    self.shape = shape
    gui:replaceChildren('layout', {layout(shape)})  -- One column in portrait, two side by side in landscape.
end]]

local function tall()
    local visible = viewport.visibleRect()
    return visible.height > visible.width
end

function Adaptive:init(entry)
    Adaptive.super.init(self, entry)
    self.last = ''
end

function Adaptive:enter()
    window.lockOrientation('any')
    self.shape = tall() and 'portrait' or 'landscape'
    self:frame{
        hint = 'Turn a phone or a tablet, or make a desktop window taller than wide: the "expand" policy shows more design units along the long side, and the screen lays itself out again.',
        code = Adaptive.code,
        focus = 'equip',
        content = {ui.column{id = 'layout', grow = 1, align = 'stretch', self:layout(self.shape)}},
    }
end

function Adaptive:exit()
    window.lockOrientation('landscape')
    Adaptive.super.exit(self)
end

-- The gear of the captain, each with its icon and its name.
Adaptive.gear = {{icon = 'interface/icons/sword.png', text = 'Cutlass'}, {icon = 'interface/themes/fantasy/icons/shield.png', text = 'Buckler'}, {icon = 'interface/themes/fantasy/icons/potion.png', text = 'Three potions'}, {icon = 'interface/themes/fantasy/icons/key.png', text = 'Brass key'}}

-- The profile holds the portrait, the name, the traits and a line about the captain, with the actions at its foot.
function Adaptive:profile(stacked)
    return ui.card{gap = 16, align = 'stretch', grow = stacked and 0 or 1,
        ui.row{gap = 24, ui.avatar{image = 'interface/images/avatar-ana.png', size = 140}, ui.column{grow = 1, gap = 4, ui.label{text = 'Ana Souza', font = 'title'}, ui.label{text = 'Captain of the Gull, level 12', color = 'textMuted'}}},
        ui.row{gap = 12, ui.badge{text = 'Navigator', tone = 'accent', solid = true}, ui.badge{text = 'Fearless', tone = 'success'}, ui.badge{text = 'Seasick', tone = 'warning'}},
        ui.label{text = 'She keeps the Gull on course through storms and reefs, and has never lost a crew.', color = 'textMuted', align = 'stretch'},
        ui.spacer{grow = 1},
        self:actions(stacked),
    }
end

function Adaptive:stats()
    return ui.card{gap = 16, align = 'stretch',
        ui.sectionTitle{text = 'Stats'},
        Adaptive.stat('Health', 0.82, 'success', '82'),
        Adaptive.stat('Stamina', 0.45, 'warning', '45'),
        Adaptive.stat('Experience', 0.6, 'accent', '60%'),
    }
end

-- The gear lies in slots of one size, two in a row, that fill the card.
function Adaptive:gearCard(stacked)
    local slots = {}
    for index, item in ipairs(Adaptive.gear) do
        slots[index] = ui.panel{gap = 8, alignItems = 'center', justify = 'center', ui.icon{image = item.icon, size = 64}, ui.label{text = item.text, font = 'caption'}}
    end
    return ui.card{gap = 16, align = 'stretch', grow = stacked and 0 or 1, ui.sectionTitle{text = 'Gear'}, ui.grid{columns = 2, gap = 12, grow = 1, align = 'stretch', children = slots}}
end

-- A stat reads as its name, its bar and its value, so no text crosses the end of the fill.
function Adaptive.stat(name, value, tone, shown)
    return ui.row{gap = 16, ui.label{text = name, width = 170}, ui.progress{value = value, tone = tone, grow = 1}, ui.label{text = shown, width = 80, textAlign = 'end', color = 'textMuted'}}
end

function Adaptive:actions(stacked)
    local press = function(event) self.last = ' You pressed ' .. event.id:sub(1, 1):upper() .. event.id:sub(2) .. '.' end
    local buttons = {
        ui.button{id = 'equip', text = 'Equip', variant = 'primary', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
        ui.button{id = 'train', text = 'Train', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
        ui.button{id = 'rest', text = 'Rest', grow = stacked and 0 or 1, align = stacked and 'stretch' or nil, onClick = press},
    }
    return stacked and ui.column{gap = 12, align = 'stretch', children = buttons} or ui.row{gap = 12, align = 'stretch', children = buttons}
end

-- Portrait stacks everything in one column, and landscape puts the profile on the left and the stats over the gear on the right, filling the screen.
function Adaptive:layout(shape)
    if shape == 'portrait' then
        return ui.column{gap = 24, align = 'stretch', self:profile(true), self:stats(), self:gearCard(true)}
    end
    return ui.row{gap = 24, grow = 1, align = 'stretch', alignItems = 'stretch',
        ui.column{grow = 1, gap = 24, self:profile(false)},
        ui.column{grow = 1, gap = 24, self:stats(), self:gearCard(false)},
    }
end

function Adaptive:update(dt)
    Adaptive.super.update(self, dt)
    local shape = tall() and 'portrait' or 'landscape'
    if shape ~= self.shape then
        self.shape = shape
        self.gui:replaceChildren('layout', {self:layout(shape)})
        self.gui:command('equip', 'focus')
    end
    local visible = viewport.visibleRect()
    self:status(string.format('The screen is laid out for %s in %.0f by %.0f design units.%s', shape, visible.width, visible.height, self.last))
end

return Adaptive
