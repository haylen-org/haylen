-- Badges and status dots in every tone, busy rings, progress bars and circular progress that fill over time, cooldowns over item icons, icons, images and avatars.
local haylen = require('haylen')
local ui = require('haylen.ui')

local Test = require('harness.test')
local layout = require('categories.interface.layout')

local Indicators = haylen.class('Indicators', Test)

Indicators.code = [[
ui.badge{text = 'New', tone = 'success', solid = true}
ui.progress{id = 'health', value = 0.72, tone = 'success', text = 'Health'}
ui.circularProgress{id = 'cooldown', variant = 'cooldown', image = 'interface/icons/sword.png', size = 96}
gui:set('cooldown', {value = left / 3, text = math.ceil(left)})  -- Shades the icon as the seconds pass.
ui.avatar{name = 'Ana Souza'}  ui.image{image = 'interface/images/landscape.png', fit = 'cover'}]]

Indicators.tones = {'neutral', 'accent', 'success', 'warning', 'danger', 'information'}
Indicators.cooldowns = {sword = 3, potion = 5, gem = 8}

function Indicators:init(entry)
    Indicators.super.init(self, entry)
    self.time = 0
    self.left = {sword = 0, potion = 0, gem = 0}
end

local function toneGrid(make)
    local nodes = {}
    for index, tone in ipairs(Indicators.tones) do
        nodes[index] = make(tone:sub(1, 1):upper() .. tone:sub(2), tone)
    end
    return ui.grid{columns = 3, gap = 10, children = nodes}
end

function Indicators:enter()
    self:frame{
        hint = 'The bars and rings fill on their own. Press Use on an ability to start its cooldown, which shades the icon and counts the seconds down.',
        code = Indicators.code,
        focus = 'use-sword',
        content = {self:columns()},
    }
end

function Indicators:ability(name)
    return ui.column{gap = 8, align = 'center',
        ui.circularProgress{id = 'cooldown-' .. name, variant = 'cooldown', image = layout.icon(name), size = 96, value = 0},
        ui.button{id = 'use-' .. name, text = 'Use', onClick = function()
            if self.left[name] <= 0 then
                self.left[name] = Indicators.cooldowns[name]
            end
        end},
    }
end

-- A picture wider than its box, shown with one fit over a panel that marks the box.
function Indicators:fit(fit, tint)
    return ui.column{gap = 4, align = 'center',
        ui.stack{width = 150, height = 110, ui.panel{align = 'stretch'}, ui.image{image = 'interface/images/landscape.png', fit = fit, tint = tint, align = 'stretch'}},
        ui.label{text = tint and 'Fit "' .. fit .. '", tinted' or 'Fit "' .. fit .. '"', font = 'caption'},
    }
end

function Indicators:columns()
    return layout.columns{
        ui.column{grow = 1, gap = 24,
            layout.section('Component "badge"', {
                toneGrid(function(text, tone) return ui.badge{text = text, tone = tone} end),
                toneGrid(function(text, tone) return ui.badge{text = text, tone = tone, solid = true} end),
            }),
            layout.section('Components "statusIndicator" and "busyIndicator"', {
                ui.row{gap = 24, ui.statusIndicator{text = 'Online'}, ui.statusIndicator{text = 'Away', tone = 'warning'}, ui.statusIndicator{text = 'Offline', tone = 'danger'}},
                ui.row{gap = 32, ui.busyIndicator{}, ui.busyIndicator{size = 64, color = 'success'}, ui.busyIndicator{size = 32, color = 'warning'}},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Component "circularProgress"', {
                ui.row{gap = 24,
                    ui.circularProgress{id = 'ring', size = 96, text = '0%'},
                    ui.circularProgress{id = 'ring-big', size = 96, thickness = 14, tone = 'success'},
                    ui.circularProgress{value = 0.6, size = 96, tone = 'danger', text = '3'},
                    ui.circularProgress{id = 'plain', variant = 'cooldown', image = layout.icon('shield'), size = 96, tone = 'warning'},
                },
            }),
            layout.section('Cooldowns', {ui.row{gap = 32, self:ability('sword'), self:ability('potion'), self:ability('gem')}}),
            layout.section('Component "progress"', {
                ui.progress{id = 'loading', text = 'Loading'},
                ui.progress{id = 'health', tone = 'success'},
                ui.progress{value = 0.35, tone = 'warning', text = 'Fuel 35%'},
            }),
        },
        ui.column{grow = 1, gap = 24,
            layout.section('Component "icon"', {
                ui.row{gap = 24, ui.icon{image = 'interface/themes/fantasy/icons/coin.png', size = 56}, ui.icon{image = 'interface/themes/fantasy/icons/heart.png', size = 56}, ui.icon{image = 'sprites/vector/bolt.svg', size = 56, color = 'warning'}, ui.icon{image = 'sprites/vector/bolt.svg', size = 56, color = 'accent'}},
            }),
            layout.section('Image fits', {
                ui.row{gap = 12, self:fit('contain'), self:fit('cover'), self:fit('fill', '#FFFFC080')},
            }),
            layout.section('Component "avatar"', {
                ui.row{gap = 16, ui.avatar{name = 'Ana Souza'}, ui.avatar{image = 'interface/images/avatar-ana.png'}, ui.avatar{image = 'interface/images/avatar-leo.png', size = 96}, ui.avatar{name = 'Leo', size = 48}},
            }),
        },
    }
end

function Indicators:update(dt)
    Indicators.super.update(self, dt)
    self.time = self.time + dt
    local fill = (self.time / 6) % 1
    local gui = self.gui
    gui:set('loading', {value = fill})
    gui:set('health', {value = 0.5 + 0.5 * math.cos(self.time), tone = math.cos(self.time) < -0.4 and 'danger' or 'success'})
    gui:set('ring', {value = fill, text = math.floor(fill * 100) .. '%'})
    gui:set('ring-big', {value = 1 - fill})
    gui:set('plain', {value = fill})
    for name, left in pairs(self.left) do
        self.left[name] = math.max(0, left - dt)
        gui:set('cooldown-' .. name, {value = self.left[name] / Indicators.cooldowns[name], text = left > 0 and math.ceil(left) or ''})
    end
end

return Indicators
