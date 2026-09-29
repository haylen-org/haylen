-- The heads-up display of a run: the survivor on the left, the day on the right, notices at the top and touch controls at the bottom.
local ui = require('haylen.ui')

local art = require('systems.art')
local config = require('config')
local preferences = require('systems.preferences')
local sound = require('systems.sound')
local widgets = require('ui.widgets')

local hud = {}
hud.__index = hud

local icons = 'tiny_swords/ui/icons/'
local tones = {['hud.fireOut'] = 'danger', ['hud.nightFalls'] = 'warning', ['hud.morning'] = 'success'}

local function touchControls()
    return ui.row{
        id = 'touch',
        align = 'stretch',
        visible = preferences.get('touch'),
        ui.touchStick{action = 'move', radius = 140, floating = true, touchOnly = true, width = 760, height = 420, align = 'end'},
        ui.spacer{grow = 1},
        ui.column{
            gap = 20,
            align = 'end',
            ui.row{
                gap = 24,
                ui.touchButton{action = 'interact', image = icons .. 'icon_02.png', size = 120, touchOnly = true},
                ui.touchButton{action = 'special', image = icons .. 'icon_06.png', size = 140, touchOnly = true},
            },
            ui.touchButton{action = 'attack', image = icons .. 'icon_05.png', size = 190, align = 'end', touchOnly = true},
        },
    }
end

function hud.new(game, onPause)
    local self = setmetatable({game = game, shown = {}}, hud)
    local class = game.player.class
    self.document = ui.mount(ui.column{
        padding = 32,
        ui.row{
            align = 'stretch',
            gap = 24,
            ui.panel{
                width = 600,
                align = 'start',
                gap = 12,
                ui.row{
                    gap = 20,
                    ui.icon{image = art.avatar(class), size = 110},
                    ui.column{
                        grow = 1,
                        gap = 10,
                        ui.progress{id = 'health', tone = 'success'},
                        ui.progress{id = 'fuel', tone = 'warning'},
                        ui.progress{id = 'special', tone = 'accent', text = widgets.text('hud.special'), visible = class.specialCooldown > 0},
                    },
                },
                ui.row{gap = 12, ui.icon{image = icons .. 'icon_02.png', size = 44}, ui.label{id = 'wood', font = 'button'}},
            },
            ui.spacer{grow = 1},
            ui.panel{
                align = 'start',
                gap = 4,
                ui.label{id = 'day', font = 'heading', align = 'end'},
                ui.label{id = 'phase', align = 'end'},
                ui.label{id = 'raiders', color = 'danger', align = 'end', visible = false},
            },
            ui.button{id = 'pause', icon = icons .. 'icon_10.png', variant = 'icon', align = 'start', onClick = function()
                sound.play('click')
                onPause()
            end},
        },
        ui.toast{id = 'notice', duration = 4},
        ui.spacer{grow = 1},
        touchControls(),
    })
    self:update()
    return self
end

-- Sets a property only when it changed, so the HUD does no work on quiet frames. Translations compare by the plain value they show.
function hud:show(id, key, value, compared)
    local shownKey = id .. '.' .. key
    compared = compared or value
    if self.shown[shownKey] ~= compared then
        self.shown[shownKey] = compared
        self.document:set(id, {[key] = value})
    end
end

function hud:update()
    local game = self.game
    local player = game.player
    local health = player.health / player.maxHealth
    self:show('health', 'value', health)
    self:show('health', 'tone', health < 0.3 and 'danger' or 'success')
    self:show('health', 'text', string.format('%d / %d', math.ceil(player.health), player.maxHealth))
    if player.class.specialCooldown > 0 then
        self:show('special', 'value', 1 - player.specialCooldown / player.class.specialCooldown)
    end
    self:show('wood', 'text', string.format('%d / %d', player.wood, player.class.capacity))

    local fuel = game.campfire.fuel / config.fire.maxFuel
    self:show('fuel', 'value', fuel)
    self:show('fuel', 'tone', fuel < 0.2 and 'danger' or 'warning')
    self:show('fuel', 'text', string.format('%d%%', math.ceil(fuel * 100)))

    local cycle = game.cycle
    local left = math.ceil((1 - cycle.progress) * config.cycle[cycle.phase])
    local time = string.format('%d:%02d', left // 60, left % 60)
    self:show('day', 'text', widgets.text('hud.day', {day = cycle.day}), cycle.day)
    self:show('phase', 'text', widgets.text('hud.next.' .. cycle.phase, {time = time}), cycle.phase .. time)

    local raiders = #game.pending
    for _, raider in ipairs(game.enemies) do
        if raider.alive then
            raiders = raiders + 1
        end
    end
    self:show('raiders', 'visible', raiders > 0)
    self:show('raiders', 'text', widgets.text('hud.raiders', {count = raiders}), raiders)
end

function hud:notice(key)
    self.document:set('notice', {text = widgets.text(key), tone = tones[key], open = true})
end

function hud:setVisible(visible)
    self.document.visible = visible
end

function hud:setTouch(visible)
    self.document:set('touch', {visible = visible})
end

function hud:destroy()
    self.document:unmount()
end

return hud
