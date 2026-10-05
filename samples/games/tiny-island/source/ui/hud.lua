-- The heads-up display of a run: the survivor with health, food, special and wood on the left, the day and the fire on the right, notices at the top and touch controls at the bottom.
local ui = require('haylen.ui')

local art = require('systems.art')
local config = require('config')
local preferences = require('systems.preferences')
local sound = require('systems.sound')
local widgets = require('ui.widgets')

local hud = {}
hud.__index = hud

local tones = {['hud.fireOut'] = 'danger', ['hud.nightFalls'] = 'warning', ['hud.morning'] = 'success', ['hud.starving'] = 'danger'}

-- A bar with its icon in front, the way every meter of the HUD reads.
local function meter(id, icon, tone, visible)
    return ui.row{gap = 12, visible = visible, ui.icon{image = art.icon(icon), size = 44}, ui.progress{id = id, tone = tone, grow = 1}}
end

local function touchControls(class)
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
                ui.touchButton{action = 'interact', image = art.icon('log'), size = 120, touchOnly = true},
                ui.touchButton{action = 'special', image = art.icon(class.specialIcon), size = 140, touchOnly = true},
            },
            ui.touchButton{action = 'attack', image = art.icon(class.attackIcon), size = 190, align = 'end', touchOnly = true},
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
                width = 620,
                align = 'start',
                gap = 10,
                ui.row{
                    gap = 20,
                    ui.icon{image = art.portrait(class), size = 132},
                    ui.column{
                        grow = 1,
                        gap = 8,
                        meter('health', 'heart', 'success'),
                        meter('food', 'meat', 'warning'),
                        meter('special', class.specialIcon, 'accent', class.specialCooldown > 0),
                    },
                },
                ui.row{gap = 12, ui.icon{image = art.icon('log'), size = 48}, ui.label{id = 'wood', font = 'button'}},
            },
            ui.spacer{grow = 1},
            ui.panel{
                width = 420,
                align = 'start',
                gap = 8,
                ui.row{gap = 12, ui.icon{id = 'phaseIcon', image = art.icon('sun'), size = 52}, ui.label{id = 'day', font = 'heading'}},
                ui.label{id = 'phase'},
                meter('fuel', 'fire', 'warning'),
                ui.row{id = 'raiders', gap = 12, visible = false, ui.icon{image = art.icon('skull'), size = 40}, ui.label{id = 'raidersText', color = 'dangerText'}},
            },
            ui.button{id = 'pause', icon = art.icon('pause'), variant = 'icon', align = 'start', onClick = function()
                sound.play('click')
                onPause()
            end},
        },
        ui.toast{id = 'notice', duration = 4},
        ui.spacer{grow = 1},
        touchControls(class),
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
    local food = player.food / config.food.max
    self:show('food', 'value', food)
    self:show('food', 'tone', food < 0.2 and 'danger' or 'warning')
    self:show('food', 'text', string.format('%d%%', math.ceil(food * 100)))
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
    self:show('phaseIcon', 'image', art.icon(game:night() and 'moon' or 'sun'))

    local raiders = #game.pending
    for _, raider in ipairs(game.enemies) do
        if raider.alive then
            raiders = raiders + 1
        end
    end
    self:show('raiders', 'visible', raiders > 0)
    self:show('raidersText', 'text', widgets.text('hud.raiders', {count = raiders}), raiders)
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
