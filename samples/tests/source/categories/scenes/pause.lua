-- Pause and process modes: `haylen.setPaused` stops the pausable world, the timer that drops its coins and the tween that moves its platform, while the pause menu runs in `whenPaused` and one timer runs `always`.
local assets = require('haylen.assets')
local graphics2d = require('haylen.graphics2d')
local haylen = require('haylen')
local input = require('haylen.input')
local scene = require('haylen.scene')
local timer = require('haylen.timer')
local tween = require('haylen.tween')
local ui = require('haylen.ui')

local Card = require('categories.scenes.card')
local SceneTest = require('categories.scenes.scene-test')
local Scenery = require('categories.effects.scenery')

local Pause = haylen.class('Pause', SceneTest)

Pause.code = [[
local PauseMenu = {transparent = true, processMode = 'whenPaused'}
function PauseMenu:enter() haylen.setPaused(true) end
function PauseMenu:exit() haylen.setPaused(false) end
scene.push(PauseMenu)
timer.every(0.1, count, {owner = self, processMode = 'always'})  -- Keeps counting while paused.]]

Pause.coinSize = 64
Pause.gravity = 1500
-- The platform is as wide as this and moves between these shares of the width of the world, on the line it stands on.
Pause.platformWidth = 420
Pause.platformTop = 230
Pause.platformHeight = 80

-- The pause menu: transparent over the world, running only while the game is paused.
local PauseMenu = haylen.class('PauseMenu', scene.Scene)

function PauseMenu:init(world)
    self.world = world
    self.transparent = true
    self.processMode = 'whenPaused'
    self.glow = {alpha = 0.3}
end

function PauseMenu:enter()
    haylen.setPaused(true)
    tween.to(self.glow, 0.6, {alpha = 1}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut', owner = self})
    self.gui = ui.mount(ui.column{
        justify = 'center',
        onCancel = function()
            self:close()
        end,
        ui.panel{
            width = 560,
            align = 'center',
            gap = 16,
            ui.label{text = 'Paused', font = 'title', textAlign = 'center', align = 'center'},
            ui.button{id = 'resume', text = 'Resume', variant = 'primary', align = 'stretch', onClick = function() self:close() end},
            ui.button{id = 'spawn', text = 'Spawn while paused', align = 'stretch', onClick = function() self.world:spawn() end},
        },
    }, {layer = 1, owner = self})
    self.gui:command('resume', 'focus')
end

function PauseMenu:exit()
    haylen.setPaused(false)
end

function PauseMenu:close()
    if not scene.transitioning() and scene.top() == self then
        scene.pop()
    end
end

function PauseMenu:update(dt)
    self.world:describe()
    if input.pressed('pause') then
        self:close()
    end
end

function PauseMenu:renderUi()
    graphics2d.beginScreen()
    graphics2d.drawRect(graphics2d.canvasBounds(), {0.02, 0.03, 0.08, 0.35 + 0.25 * self.glow.alpha})
end

function Pause:init(entry)
    Pause.super.init(self, entry)
    self.coins = {}
    self.spins = 0
    self.always = 0
    self.dropped = 0
    self.platform = {x = -500}
    self.scenery = Scenery()
    self.coin = assets.texture('physics/coin.png', {filter = 'linear'})
    self.font = assets.font('fonts/lilita-one-regular.ttf')
    self.platformSlice = graphics2d.newNineSlice(assets.texture('input/platform.png', {filter = 'linear'}), {borders = {170, 0, 170, 0}})
end

function Pause:enter()
    self:frame{
        hint = 'Pause with the button, P or the Y button. The coins, the timer that drops them and the tween of the platform stop, the "always" timer keeps counting and the menu animates.',
        code = Pause.code,
        view = Card.world,
        play = true,
        controls = {ui.button{id = 'pause', text = 'Pause', onClick = function() self:openMenu() end}},
    }
    timer.every(0.4, function() self:spawn() end, {owner = self})
    timer.every(0.1, function() self.always = self.always + 0.1 end, {owner = self, processMode = 'always'})
    tween.to(self.platform, 2, {x = 500}, {loopMode = 'yoyo', repeatCount = -1, ease = 'sineInOut', owner = self})
end

function Pause:openMenu()
    if not scene.transitioning() and scene.top() == self then
        scene.push(PauseMenu(self))
    end
end

-- Drops a coin from above the top of the world. The spawner timer calls it while the game runs, and the menu can call it while paused.
function Pause:spawn()
    self.dropped = self.dropped + 1
    self.coins[#self.coins + 1] = {x = -800 + (self.dropped * 337) % 1600, y = -620, speed = 0, spin = self.dropped}
    if #self.coins > 40 then
        table.remove(self.coins, 1)
    end
end

-- Shows the state in the status line, from the update of this test while it runs and from the update of the menu while the game is paused.
function Pause:describe()
    local text = haylen.paused() and string.format('The game is paused, and the "always" timer counts on: %.1f seconds.', self.always) or string.format('The game runs: %d coins dropped, and the "always" timer counts %.1f seconds.', self.dropped, self.always)
    if text ~= self.shown then
        self.shown = text
        self:set('status', {text = text})
    end
end

function Pause:update(dt)
    Pause.super.update(self, dt)
    if input.pressed('pause') then
        self:openMenu()
    end
    self.spins = self.spins + 1
    local half = Pause.platformWidth / 2
    for _, coin in ipairs(self.coins) do
        coin.spin = coin.spin + dt * 4
        local before = coin.y
        coin.speed = coin.speed + Pause.gravity * dt
        coin.y = coin.y + coin.speed * dt
        local bottom = Pause.coinSize / 2
        if coin.on then
            coin.x = coin.x + self.platform.x - coin.on
            coin.on = self.platform.x
            if math.abs(coin.x - self.platform.x) > half then
                coin.on = nil
            else
                coin.y, coin.speed = Pause.platformTop - bottom, 0
            end
        elseif before + bottom <= Pause.platformTop and coin.y + bottom >= Pause.platformTop and math.abs(coin.x - self.platform.x) <= half then
            coin.y, coin.speed, coin.on = Pause.platformTop - bottom, 0, self.platform.x
        end
        if coin.y + bottom >= Scenery.ground then
            coin.y, coin.speed = Scenery.ground - bottom, 0
        end
    end
    self:describe()
end

-- Draws a timer as a pill with a ring that fills over each second, its name and its count.
function Pause:drawTimer(text, fraction, x, y, running)
    local width = graphics2d.measureText(self.font, text, {size = 36}) + 110
    graphics2d.drawShape({x, y, width, 72}, {radius = 36, color = '#E62F3352', borderWidth = 3, borderColor = running and '#FF4CD45A' or '#FF8A90B0', layer = 5})
    graphics2d.drawRing(x + 38, y + 36, 20, 6, '#FF15172A', {layer = 6})
    graphics2d.drawArc(x + 38, y + 36, 20, 6, -math.pi / 2, -math.pi / 2 + math.pi * 2 * fraction, running and '#FF4CD45A' or '#FF8A90B0', {layer = 7})
    graphics2d.drawText(self.font, text, x + 74, y + 36, {size = 36, anchor = {0, 0.5}, outlineWidth = 3, outlineColor = '#FF15172A', layer = 7})
end

function Pause:draw(area)
    self.scenery:draw(false)
    local platform = {self.platform.x - Pause.platformWidth / 2, Pause.platformTop - Pause.platformHeight * 22 / 241, Pause.platformWidth, Pause.platformHeight}
    graphics2d.drawNineSlice(self.platformSlice, platform, nil, {borderScale = Pause.platformHeight / 241, layer = 1})
    for _, coin in ipairs(self.coins) do
        graphics2d.draw(self.coin, coin.x, coin.y, {width = Pause.coinSize * math.max(0.15, math.abs(math.cos(coin.spin))), height = Pause.coinSize, layer = 2})
    end
    local bounds = graphics2d.canvasBounds()
    local left, top = bounds.x + 40, bounds.y + 40
    self:drawTimer(string.format('Always: %.1f s', self.always), self.always % 1, left, top, true)
    self:drawTimer(string.format('Pausable: %d coins', self.dropped), haylen.paused() and 0 or (haylen.elapsed() % 0.4) / 0.4, left, top + 92, not haylen.paused())
end

return Pause
