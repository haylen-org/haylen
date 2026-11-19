-- The only scene: it runs the quest, answers the buttons and the mouse, and every frame tells the window which parts keep the mouse.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local ui = require('haylen.ui')
local window = require('haylen.window')

local art = require('systems.art')
local config = require('config')
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
    self.stage = stage.new(images, self.desktop:area())
    self.effects = effects.new(images)
    self.hud = hud.new(self, self.desktop.canStrip, {
        sword = function() self.quest:upgradeSword() end,
        potion = function() self.quest:drinkPotion() end,
        mode = function() self.desktop:toggle() end,
        quit = function() haylen.quit() end,
    })
    self.quest = quest.new(self, images, self.effects, self.hud, self.stage.lane)
end

-- A press on the ground or the grip drags the window of a desktop, which has to start while the button is still down.
function game:event(event)
    if not self.desktop.canStrip or event.type ~= 'mouseDown' or event.button ~= 'left' or ui.usingPointer() then
        return
    end
    if self.quest:targetAt(event.x, event.y) == nil and self.stage:grabs(event.x, event.y) then
        window.startDrag()
    end
end

function game:update(dt)
    if input.pressed('quit') and not self.desktop.strip and haylen.canQuit() then
        haylen.quit()
        return
    end

    local area = self.desktop:area()
    self.stage:layout(area)
    self.hud:place(area)
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

-- On a page, the quest draws inside the framed window that the page shows around it.
function game:render()
    graphics2d.beginScreen()
    if self.desktop.canStrip then
        self:drawQuest()
        return
    end

    local area = self.desktop:area()
    self.desktop:drawPage(area)
    graphics2d.pushMask(function() self.desktop:drawWindowShape(area) end, {layer = config.layer.backdrop})
    self:drawQuest()
    graphics2d.popMask()
    self.desktop:drawFrame(area)
end

function game:drawQuest()
    self.stage:draw(not self.desktop.strip, self.desktop.canStrip, haylen.elapsed())
    self.quest:draw()
    self.effects:draw()
end

return game
