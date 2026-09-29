-- The only scene: it runs the quest, answers the buttons and the mouse, and every frame tells the window which parts keep the mouse.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local art = require('systems.art')
local desktop = require('systems.desktop')
local effects = require('systems.effects')
local hud = require('ui.hud')
local quest = require('systems.quest')
local stage = require('systems.stage')

local game = {}
game.__index = game

function game.new()
    return setmetatable({}, game)
end

function game:enter()
    local images = art.load()
    self.desktop = desktop.new(self)
    self.stage = stage.new(images)
    self.effects = effects.new()
    self.hud = hud.new(self, {
        sword = function() self.quest:upgradeSword() end,
        potion = function() self.quest:drinkPotion() end,
        mode = function() self.desktop:toggle() end,
        quit = function() haylen.quit() end,
    })
    self.quest = quest.new(self, images, self.effects, self.hud, self.stage.lane)
end

-- A press on the ground or the grip drags the window, which has to start while the button is still down.
function game:event(event)
    if event.type ~= 'mouse_down' or event.button ~= 'left' or ui.wantsPointer() then
        return
    end
    if self.quest:targetAt(event.x, event.y) == nil and self.stage:grabs(event.x, event.y) then
        window.startDrag()
    end
end

function game:update(dt)
    if input.pressed('quit') and not self.desktop.strip then
        haylen.quit()
        return
    end

    self.stage:layout()
    if input.pressed('strike') then
        self.quest:strikeAt(input.mousePosition())
    end
    self.quest:update(dt, self.stage.lane)
    self.effects:update(dt)
    self.hud:refresh(self.quest, self.desktop.strip)

    local regions = {}
    self.stage:collectRegions(regions)
    self.quest:collectRegions(regions)
    self.hud:collectRegions(regions)
    self.desktop:keepMouse(regions)
end

function game:render()
    graphics2d.beginScreen()
    self.stage:draw(not self.desktop.strip, haylen.time())
    self.quest:draw()
    self.effects:draw()
end

return game
