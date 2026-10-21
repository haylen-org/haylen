-- The player data the events tests share: an autoload that `app.json` lists, so it loads before `main.lua`, lives as long as the app and gets the callbacks of the engine. It keeps the coins, the hats bought in the shop, the time played and the input events it saw, and draws a coin counter in the top-right corner of the play area while an events test shows.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')

local playerData = {coins = 30, hats = {}, playTime = 0, inputs = 0, startFrame = -1, counterShown = false, counterCorner = nil, processMode = 'always'}

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

-- Draws the coin and the count on a dark pill whose top-right corner sits 20 units inside the corner of the play area.
function playerData:renderUi()
    local corner = self.counterCorner
    if not self.counterShown or not corner then
        return
    end
    self.coin = self.coin or assets.texture('physics/coin.png', {filter = 'linear'})
    self.font = self.font or assets.font('fonts/lilita-one-regular.ttf')
    local text = self.coins .. (self.coins == 1 and ' coin' or ' coins')
    local width = graphics2d.measureText(self.font, text, {size = 30})
    local right, top, height = corner[1] - 20, corner[2] + 20, 56
    local left = right - width - 92
    graphics2d.beginScreen()
    graphics2d.drawShape({left, top, right - left, height}, {radius = height / 2, color = '#E6151A2A', borderWidth = 2, borderColor = '#FFFFC93C'})
    graphics2d.draw(self.coin, left + 30, top + height / 2, {width = 52, height = 52, layer = 1})
    graphics2d.drawText(self.font, text, left + 64, top + height / 2, {size = 30, color = '#FFFFE9A8', anchor = {0, 0.5}, outlineWidth = 3, outlineColor = '#FF15172A', layer = 2})
end

return playerData
