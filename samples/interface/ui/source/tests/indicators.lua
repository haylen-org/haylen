-- Indicators: badges and status dots in every tone, busy rings, progress bars and circular progress that fill over time, cooldowns over item icons, icons, images and avatars.
local haylen = require('haylen')
local ui = require('haylen.ui')

local sample = require('sample')

local Indicators = haylen.class('Indicators', sample.Test)

Indicators.hints = 'The bars and rings fill on their own. Press Use on an ability to start its cooldown, which shades the icon and counts the seconds down.'
Indicators.focus = 'use-sword'

local kTones = {'neutral', 'accent', 'success', 'warning', 'danger', 'information'}
local kCooldowns = {sword = 3, potion = 5, gem = 8}

function Indicators:init(entry)
    Indicators.super.init(self, entry)
    self.time = 0
    self.cooldowns = {sword = 0, potion = 0, gem = 0}
end

local function toneRow(make)
    local nodes = {}
    for index, tone in ipairs(kTones) do
        nodes[index] = make(tone)
    end
    return ui.row{gap = 10, children = nodes}
end

function Indicators:ability(name)
    return ui.column{gap = 8, align = 'center',
        ui.circularProgress{id = 'cooldown-' .. name, style = 'cooldown', image = 'icons/' .. name .. '.png', size = 96, value = 0},
        ui.button{id = 'use-' .. name, text = 'Use', onClick = function()
            if self.cooldowns[name] <= 0 then
                self.cooldowns[name] = kCooldowns[name]
            end
        end},
    }
end

-- A picture wider than its box, shown with one fit over a panel that marks the box.
function Indicators:fit(fit, tint)
    return ui.column{gap = 4, align = 'center',
        ui.stack{width = 150, height = 110, ui.panel{align = 'stretch'}, ui.image{image = 'images/landscape_day.png', fit = fit, tint = tint, align = 'stretch'}},
        ui.label{text = tint and fit .. ' with a tint' or fit, font = 'caption'},
    }
end

function Indicators:content()
    return sample.columns{
        ui.column{grow = 1, gap = 24,
            sample.section('badge', {
                toneRow(function(tone) return ui.badge{text = tone, tone = tone} end),
                toneRow(function(tone) return ui.badge{text = tone, tone = tone, solid = true} end),
            }),
            sample.section('statusIndicator and busyIndicator', {
                ui.row{gap = 24, ui.statusIndicator{text = 'Online'}, ui.statusIndicator{text = 'Away', tone = 'warning'}, ui.statusIndicator{text = 'Offline', tone = 'danger'}},
                ui.row{gap = 32, ui.busyIndicator{}, ui.busyIndicator{size = 64, color = 'success'}, ui.busyIndicator{size = 32, color = 'warning'}},
            }),
            sample.section('progress', {
                ui.progress{id = 'loading', text = 'Loading'},
                ui.progress{id = 'health', tone = 'success'},
                ui.progress{value = 0.35, tone = 'warning', text = 'Fuel 35%'},
            }),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('circularProgress', {
                ui.row{gap = 24,
                    ui.circularProgress{id = 'ring', text = '0%'},
                    ui.circularProgress{id = 'ring-big', size = 120, thickness = 16, tone = 'success'},
                    ui.circularProgress{value = 0.6, tone = 'danger', text = '3'},
                    ui.circularProgress{id = 'plain', style = 'cooldown', size = 96, tone = 'warning'},
                },
            }),
            sample.section('cooldowns', {ui.row{gap = 32, self:ability('sword'), self:ability('potion'), self:ability('gem')}}),
        },
        ui.column{grow = 1, gap = 24,
            sample.section('icon', {
                ui.row{gap = 16, ui.icon{image = 'icons/coin.png'}, ui.icon{image = 'icons/heart.png', size = 56}, ui.icon{image = 'icons/star.png', size = 56, color = 'accent'}, ui.icon{image = 'icons/gear.png', size = 72, color = 'textMuted'}},
            }),
            sample.section('image fits', {
                ui.row{gap = 12, self:fit('contain'), self:fit('cover'), self:fit('fill', '#FFFFC080')},
            }),
            sample.section('avatar', {
                ui.row{gap = 16, ui.avatar{name = 'Ana Souza'}, ui.avatar{image = 'images/avatar_ana.png'}, ui.avatar{image = 'images/avatar_leo.png', size = 96}, ui.avatar{name = 'Leo', size = 48}},
            }),
        },
    }
end

function Indicators:update(dt)
    self.time = self.time + dt
    local fill = (self.time / 6) % 1
    local document = self.document
    document:set('loading', {value = fill})
    document:set('health', {value = 0.5 + 0.5 * math.cos(self.time), tone = math.cos(self.time) < -0.4 and 'danger' or 'success'})
    document:set('ring', {value = fill, text = math.floor(fill * 100) .. '%'})
    document:set('ring-big', {value = 1 - fill})
    document:set('plain', {value = fill})
    for name, left in pairs(self.cooldowns) do
        self.cooldowns[name] = math.max(0, left - dt)
        document:set('cooldown-' .. name, {value = self.cooldowns[name] / kCooldowns[name], text = left > 0 and math.ceil(left) or ''})
    end
end

return Indicators
