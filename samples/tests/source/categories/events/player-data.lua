-- The player data the events tests share: an autoload that `app.json` lists, so it loads before `main.lua`, lives as long as the app and gets the callbacks of the engine. It keeps the coins, the hats bought in the shop, the time played and the input events it saw, and draws the coins in the top right corner of the screen while an events test shows.
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local viewport = require('haylen.viewport')

local playerData = {coins = 30, hats = {}, playTime = 0, inputs = 0, startFrame = -1, counterShown = false, processMode = 'always'}

function playerData:start()
    self.startFrame = haylen.frameIndex()
end

function playerData:event(event)
    if event.type == 'keyDown' or event.type == 'mouseDown' or event.type == 'touchBegan' then
        self.inputs = self.inputs + 1
    end
end

function playerData:update(dt)
    self.playTime = self.playTime + dt
end

function playerData:earn(amount)
    self.coins = self.coins + amount
end

-- Spends the price when there are enough coins and returns whether it did.
function playerData:spend(price)
    if self.coins < price then
        return false
    end
    self.coins = self.coins - price
    return true
end

function playerData:renderUi()
    if not self.counterShown then
        return
    end
    local safe = viewport.safeRect()
    graphics2d.beginScreen()
    graphics2d.drawCircle(safe:right() - 150, safe.y + 44, 14, '#FFF2B23A')
    graphics2d.drawText(nil, tostring(self.coins), safe:right() - 126, safe.y + 44, {size = 30, color = '#FFF7CB70', anchor = {0, 0.5}})
end

return playerData
