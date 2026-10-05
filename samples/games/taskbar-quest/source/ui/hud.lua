-- The counters in the top left corner of the safe area and the buttons in its top right corner.
local m = require('haylen.math')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local config = require('config')

local hud = {}
hud.__index = hud

function hud.new(owner, actions)
    local self = setmetatable({shown = {}}, hud)
    self.gui = ui.mount(ui.stack{
        ui.panel{
            id = 'stats',
            anchor = 'topLeft',
            margin = 8,
            ui.row{
                gap = 8,
                minHeight = 44,
                ui.icon{id = 'coin', image = 'images/icon_coin.png'},
                ui.label{id = 'gold', text = 0, font = 'button', minWidth = 40},
                ui.icon{image = 'images/icon_skull.png'},
                ui.label{id = 'defeated', text = 0, font = 'button', minWidth = 40},
                ui.icon{image = 'images/icon_sword.png'},
                ui.label{id = 'level', text = 1, font = 'button', minWidth = 24},
                ui.icon{image = 'images/icon_heart.png'},
                ui.progress{id = 'health', tone = 'success', width = 150},
            },
        },
        ui.panel{
            id = 'actions',
            anchor = 'topRight',
            margin = 8,
            ui.row{
                gap = 6,
                focusable = false,
                ui.button{id = 'sword', icon = 'images/icon_sword.png', onClick = actions.sword},
                ui.button{id = 'potion', icon = 'images/icon_potion.png', onClick = actions.potion},
                ui.button{id = 'mode', onClick = actions.mode},
                ui.button{id = 'quit', icon = 'images/icon_close.png', variant = 'icon', tooltip = 'Quit', onClick = actions.quit},
            },
        },
    }, {owner = owner})
    return self
end

-- Sets the properties of a node only when the value behind them changed, instead of on every frame.
function hud:show(id, value, properties)
    if self.shown[id] ~= value then
        self.shown[id] = value
        self.gui:set(id, properties)
    end
end

function hud:refresh(quest, strip)
    local health = quest.hero.health
    local share = health / config.hero.health
    local cost = quest:swordCost()
    local canUpgrade = quest.gold >= cost
    local canDrink = quest:canDrinkPotion()
    self:show('gold', quest.gold, {text = quest.gold})
    self:show('defeated', quest.defeated, {text = quest.defeated})
    self:show('level', quest.sword, {text = quest.sword})
    self:show('health', health, {value = share, text = health .. ' / ' .. config.hero.health, tone = share > 0.5 and 'success' or (share > 0.25 and 'warning' or 'danger')})
    self:show('sword', cost .. tostring(canUpgrade), {text = cost .. 'g', enabled = canUpgrade, tooltip = 'Sword level ' .. (quest.sword + 1) .. ' for ' .. cost .. ' gold'})
    self:show('potion', canDrink, {text = config.shop.potion .. 'g', enabled = canDrink, tooltip = 'A potion heals ' .. config.shop.heal .. ' for ' .. config.shop.potion .. ' gold'})
    self:show('mode', strip, {
        text = strip and 'Window' or 'Strip',
        icon = strip and 'images/icon_window.png' or 'images/icon_strip.png',
        tooltip = strip and 'Show the quest in a normal window' or 'Send the quest back above the taskbar',
    })
end

-- Returns the center of the coin icon, where the coins fly to.
function hud:counter()
    local area = self.gui:bounds('coin')
    return area.x + area.width / 2, area.y + area.height / 2
end

function hud:pulse(id)
    tween.fromTo(self.gui:transform(id), 0.25, {scale = m.vec2(1.5, 1.5)}, {scale = m.vec2(1, 1)}, {ease = 'quadOut', overwrite = true})
end

-- Adds the two panels, once drawn, to the regions that keep the mouse.
function hud:collectRegions(regions)
    for _, id in ipairs({'stats', 'actions'}) do
        local area = self.gui:bounds(id)
        if area then
            regions[#regions + 1] = area
        end
    end
end

return hud
